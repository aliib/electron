// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_websocket.h"
#include "atom/browser/api/atom_api_session.h"
#include "atom/browser/net/atom_websocket_channel.h"
#include "atom/common/native_mate_converters/buffer_converter.h"
#include "atom/common/native_mate_converters/gurl_converter.h"
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
  GURL url;
  args->GetNext(&url);
  std::vector<std::string> protocols;
  args->GetNext(&protocols);
  
  auto session = Session::FromPartition(isolate, "");
  auto browser_context = session->browser_context();
  auto websocket =  new WebSocket(args->isolate(), args->GetThis());
  websocket->atom_websocket_channel_ = AtomWebSocketChannel::Create(
    browser_context,
    std::move(url),
    std::move(protocols));
  websocket->Pin();
  return websocket;
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


void WebSocket::Send(scoped_refptr<net::IOBufferWithSize> buffer,
  bool is_last) {
  atom_websocket_channel_->Send(buffer, is_last);
}

void WebSocket::Close() {
}


void WebSocket::Pin() {
  if (wrapper_.IsEmpty()) {
    wrapper_.Reset(isolate(), GetWrapper());
  }
}

void WebSocket::Unpin() {
  wrapper_.Reset();
}


}  // namespace api

}  // namespace atom
