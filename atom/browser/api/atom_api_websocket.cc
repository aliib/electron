// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_websocket.h"
#include "native_mate/dictionary.h"

namespace atom {

namespace api {

WebSocket::WebSocket(v8::Isolate* isolate, v8::Local<v8::Object> wrapper) {
  InitWith(isolate, wrapper);
}

WebSocket::~WebSocket() {
}

// static
mate::WrappableBase* WebSocket::New(mate::Arguments* args) {
  auto isolate = args->isolate();
  std::string url;
  args->GetNext(&url);
  std::string protocols;
  args->GetNext(&protocols);
  return new WebSocket(args->isolate(), args->GetThis());
}

// static
void WebSocket::BuildPrototype(v8::Isolate* isolate,
  v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "WebSocket"));
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
    // Request API
    .SetMethod("send", &WebSocket::Send)
    .SetMethod("close", &WebSocket::Close);
}


void WebSocket::Send() {
}

void WebSocket::Close() {
}

}  // namespace api

}  // namespace atom
