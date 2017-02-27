// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_websocket.h"
#include "atom/browser/api/atom_api_session.h"
#include "atom/browser/net/atom_websocket_channel.h"
#include "atom/common/api/event_emitter_caller.h"
#include "atom/common/native_mate_converters/buffer_converter.h"
#include "atom/common/native_mate_converters/gurl_converter.h"
#include "atom/common/native_mate_converters/net_converter.h"
#include "atom/common/native_mate_converters/value_converter.h"
#include "atom/common/node_includes.h"
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
    std::move(protocols),
    websocket);
  websocket->Pin();
  return websocket;
}

// static
void WebSocket::BuildPrototype(v8::Isolate* isolate,
  v8::Local<v8::FunctionTemplate> prototype) {
  prototype->SetClassName(mate::StringToV8(isolate, "WebSocket"));
  mate::ObjectTemplateBuilder(isolate, prototype->PrototypeTemplate())
    // Request API
    .SetMethod("_send", &WebSocket::Send)
    .SetMethod("_close", &WebSocket::Close);
}


void WebSocket::Send(scoped_refptr<net::IOBufferWithSize> buffer,
  net::WebSocketFrameHeader::OpCodeEnum op_code,
  bool is_last) {
  atom_websocket_channel_->Send(buffer, op_code, is_last);
}

void WebSocket::Close(uint32_t code, const std::string& reason) {
  atom_websocket_channel_->Close(static_cast<uint16_t>(code), reason);
}


void WebSocket::OnFinishOpeningHandshake(
  std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  v8::HandleScope handle_scope(isolate());
  mate::EmitEvent(isolate(), GetWrapper(), "open");
}

void WebSocket::OnDataFrame(bool fin,
  net::WebSocketFrameHeader::OpCodeEnum type,
  scoped_refptr<net::IOBuffer> buffer,
  size_t buffer_size) {
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Value> data = node::Buffer::New(isolate(),
    buffer->data(), buffer_size, nullptr, nullptr).ToLocalChecked();
  mate::EmitEvent(isolate(), GetWrapper(), "message", data);
}


void WebSocket::OnDropChannel(bool was_clean, uint32_t code,
  const std::string& reason) {
  v8::HandleScope handle_scope(isolate());
  mate::EmitEvent(isolate(), GetWrapper(), "close", code, reason);
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
