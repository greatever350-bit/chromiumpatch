/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 */

#include "third_party/blink/renderer/modules/plugins/dom_plugin_array.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/modules/plugins/dom_mime_type_array.h"
#include "third_party/blink/renderer/modules/plugins/navigator_plugins.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {
DOMPlugin* MakeFakePlugin(const String& plugin_name, LocalDOMWindow* window) {
  String description, filename;
  // Map plugin names to realistic descriptions and filenames.
  if (plugin_name == "Chrome PDF Plugin") {
    description = "Portable Document Format";
    filename = "internal-pdf-viewer";
  } else if (plugin_name == "Chrome PDF Viewer") {
    description = "PDF Viewer";
    filename = "mhjfbmdgcfjbbpaeojofohoefgiehjai";
  } else if (plugin_name == "Native Client") {
    description = "Native Client Executable";
    filename = "internal-nacl-plugin";
  } else if (plugin_name == "Chrome Remote Desktop") {
    description = "Remote Desktop";
    filename = "internal-remoting-viewer";
  } else if (plugin_name == "Widevine Content Decryption Module") {
    description = "Widevine CDM";
    filename = "internal-widevine-cdm";
  } else {
    description = "Plugin";
    filename = "plugin";
  }

  auto* plugin_info =
      MakeGarbageCollected<PluginInfo>(plugin_name, filename, description,
                                       /*background_color=*/Color::kTransparent,
                                       /*may_use_external_handler=*/false);
  // Add typical MIME types for each plugin (optional, but improves realism).
  Vector<String> extensions;
  if (plugin_name == "Chrome PDF Plugin" || plugin_name == "Chrome PDF Viewer") {
    extensions.push_back("pdf");
    for (const char* mime : {"application/pdf", "text/pdf"}) {
      auto* mime_info = MakeGarbageCollected<MimeClassInfo>(
          mime, description, *plugin_info, extensions);
      plugin_info->AddMimeType(mime_info);
    }
  } else if (plugin_name == "Native Client") {
    // NaCl has its own MIME type.
    auto* mime_info = MakeGarbageCollected<MimeClassInfo>(
        "application/x-nacl", description, *plugin_info, extensions);
    plugin_info->AddMimeType(mime_info);
  } else if (plugin_name == "Widevine Content Decryption Module") {
    auto* mime_info = MakeGarbageCollected<MimeClassInfo>(
        "application/x-ppapi-widevine-cdm", description, *plugin_info, extensions);
    plugin_info->AddMimeType(mime_info);
  }
  // Add others as needed.
  return MakeGarbageCollected<DOMPlugin>(window, *plugin_info);
}
}  // namespace

DOMPluginArray::DOMPluginArray(LocalDOMWindow* window) : window_(window) {
  // Force the hardcoded list regardless of PDF viewer availability.
  // Use the exact 5 plugins that a real Chrome installation reports.
  Vector<String> plugins{
      "Chrome PDF Plugin",
      "Chrome PDF Viewer",
      "Native Client",
      "Chrome Remote Desktop",
      "Widevine Content Decryption Module"
  };
  for (auto name : plugins) {
    dom_plugins_.push_back(MakeFakePlugin(name, window));
  }
}

void DOMPluginArray::Trace(Visitor* visitor) const {
  visitor->Trace(window_);
  visitor->Trace(dom_plugins_);
  ScriptWrappable::Trace(visitor);
}

unsigned DOMPluginArray::length() const {
  return dom_plugins_.size();
}

DOMPlugin* DOMPluginArray::item(unsigned index) {
  if (index >= dom_plugins_.size()) {
    return nullptr;
  }
  return dom_plugins_[index].Get();
}

DOMPlugin* DOMPluginArray::namedItem(const AtomicString& property_name) {
  for (const auto& plugin : dom_plugins_) {
    if (plugin->name() == property_name) {
      return plugin.Get();
    }
  }
  return nullptr;
}

void DOMPluginArray::NamedPropertyEnumerator(Vector<String>& property_names,
                                             ExceptionState&) const {
  property_names.ReserveInitialCapacity(dom_plugins_.size());
  for (const auto& plugin : dom_plugins_) {
    property_names.UncheckedAppend(plugin->name());
  }
}

bool DOMPluginArray::NamedPropertyQuery(const AtomicString& property_name,
                                        ExceptionState& exception_state) const {
  Vector<String> properties;
  NamedPropertyEnumerator(properties, exception_state);
  return properties.Contains(property_name);
}

void DOMPluginArray::refresh(bool reload) {
  if (!window_) {
    return;
  }
  if (PluginData* data = GetPluginData()) {
    data->ResetPluginData();
  }
  if (reload && window_->GetFrame()) {
    window_->GetFrame()->Reload(WebFrameLoadType::kReload);
  }
}

PluginData* DOMPluginArray::GetPluginData() const {
  return (window_ && window_->GetFrame()) ? window_->GetFrame()->GetPluginData()
                                          : nullptr;
}

HeapVector<Member<DOMMimeType>> DOMPluginArray::GetFixedMimeTypeArray() {
  HeapVector<Member<DOMMimeType>> mimetypes;
  if (dom_plugins_.empty())
    return mimetypes;
  DCHECK_EQ(dom_plugins_[0]->length(), 2u);
  mimetypes.push_back(dom_plugins_[0]->item(0));
  mimetypes.push_back(dom_plugins_[0]->item(1));
  return mimetypes;
}

bool DOMPluginArray::IsPdfViewerAvailable() {
  auto* data = GetPluginData();
  if (!data)
    return false;
  for (const Member<MimeClassInfo>& mime_info : data->Mimes()) {
    if (mime_info->Type() == "application/pdf")
      return true;
  }
  return false;
}

}  // namespace blink
