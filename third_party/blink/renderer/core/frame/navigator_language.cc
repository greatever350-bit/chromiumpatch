// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/navigator_language.h"

#include "base/command_line.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Vector<String> ParseAndSanitize(const String& accept_languages) {
  Vector<String> languages = accept_languages.SplitSkippingEmpty(',');

  for (wtf_size_t i = 0; i < languages.size(); ++i) {
    String& token = languages[i];
    token = token.StripWhiteSpace();
    if (token.length() >= 3 && token[2] == '_')
      token.replace(2, 1, "-");
  }

  if (languages.empty())
    languages.push_back(DefaultLanguage());

  return languages;
}

NavigatorLanguage::NavigatorLanguage(ExecutionContext* execution_context)
    : execution_context_(execution_context) {}

AtomicString NavigatorLanguage::language() {
  return AtomicString(languages().front());
}

const Vector<String>& NavigatorLanguage::languages() {
  EnsureUpdatedLanguage();
  return languages_;
}

bool NavigatorLanguage::IsLanguagesDirty() const {
  String accept_languages_override;
  probe::ApplyAcceptLanguageOverride(execution_context_,
                                     &accept_languages_override);
  return languages_dirty_ || !accept_languages_override.IsNull();
}

void NavigatorLanguage::SetLanguagesDirty() {
  languages_dirty_ = true;
  languages_.clear();
}

void NavigatorLanguage::SetLanguagesForTesting(const String& languages) {
  languages_ = ParseAndSanitize(languages);
}

void NavigatorLanguage::EnsureUpdatedLanguage() {
  String accept_languages_override;
  probe::ApplyAcceptLanguageOverride(execution_context_,
                                     &accept_languages_override);
  if (!accept_languages_override.IsNull()) {
    languages_ = ParseAndSanitize(accept_languages_override);
    languages_dirty_ = true;
    return;
  }

  if (languages_dirty_) {
    languages_ = ParseAndSanitize(GetAcceptLanguages());
    // PATCH: Reduction logic removed – full list is kept.
    languages_dirty_ = false;
  }
}

void NavigatorLanguage::Trace(Visitor* visitor) const {
  visitor->Trace(execution_context_);
}

}  // namespace blink
