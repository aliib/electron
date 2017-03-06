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
#include "net/http/http_request_headers.h"

namespace mate {

template<>
struct Converter<atom::api::WebSocket::State> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
    atom::api::WebSocket::State state) {
    return Converter<uint32_t>::ToV8(isolate, static_cast<uint32_t>(state));
  }
};

} // namespace mate

namespace atom {

namespace api {

WebSocket::WebSocket(v8::Isolate* isolate, v8::Local<v8::Object> wrapper, 
  const GURL& url)
  : state_(State::CONNECTING)
  , buffered_amount_(0)
  , url_(url) {
  InitWith(isolate, wrapper);
}

WebSocket::~WebSocket() {
}

// static
mate::WrappableBase* WebSocket::New(mate::Arguments* args) {
  auto isolate = args->isolate();

  if (args->Length() == 0) {
    args->ThrowError("A URL is required for WebSocket constructor");
    return nullptr;
  }

  GURL url;
  if (!args->GetNext(&url)) {
    args->ThrowTypeError("First argument to WebSocket constructor must be a valid URL");
    return nullptr;
  }

  std::string session = "";
  std::vector<std::string> protocols;
  GURL origin;
  std::string additional_headers;

  auto next = args->PeekNext();
  if (next.IsEmpty()) {
    // JS code: ws = new WebSocket('ws://blabla.com')
    return NewInternal(args->isolate(),
      args->GetThis(),
      std::move(session),
      std::move(url),
      std::move(protocols),
      std::move(origin),
      std::move(additional_headers));
  }

  if (next->IsString()) {
    // JS code: ws = new WebSocket('ws://blabla.com', 'chat')
    std::string protocol;
    if (!args->GetNext(&protocol)) {
      args->ThrowTypeError("Second argument does not seem to be a valid protocol string");
      return nullptr;
    }
    protocols.push_back(std::move(protocol));
    next = args->PeekNext();
  } else if (next->IsArray()) {
    // JS code: ws = new WebSocket('ws://blabla.com', ['chat', 'superchat'])
    if (!args->GetNext(&protocols)) {
      args->ThrowTypeError("Second argument must be an array of strings");
      return nullptr;
    }
    next = args->PeekNext();
  } 
  
  if (next.IsEmpty()) {
    // User did not specify an options object.
    return NewInternal(args->isolate(),
      args->GetThis(),
      std::move(session),
      std::move(url),
      std::move(protocols),
      std::move(origin),
      std::move(additional_headers));
  }

  // JS code: ws = new WebSocket('ws://blabla.com', {
  //  headers: {
  //    'Additional-Header': Some-Value
  //  }
  // })
  if (next->IsObject()) {
    mate::Dictionary options;
    if (!args->GetNext(&options)) {
      args->ThrowTypeError("Options does not seem to be a valid object");
      return nullptr;
    }

    // session option
    options.Get("session", &session);

    // headers option
    mate::Dictionary headers_dict;
    std::map<std::string, std::string> headers_map;
    if (options.Get("headers", &headers_dict) &&
      mate::ConvertFromV8(isolate, headers_dict.GetHandle(), &headers_map)) {
      net::HttpRequestHeaders headers;
      for (auto& pair : headers_map) {
        headers.SetHeader(pair.first, std::move(pair.second));
      }
      additional_headers = headers.ToString();
    }

    // origin option
    options.Get("origin", &origin);
  }

   return NewInternal(args->isolate(),
     args->GetThis(),
     std::move(session),
     std::move(url),
     std::move(protocols),
     std::move(origin),
     std::move(additional_headers));
}

mate::WrappableBase* WebSocket::NewInternal(v8::Isolate* isolate,
  v8::Local<v8::Object> wrapper,
  std::string&& sessionName,
  GURL&& url,
  std::vector<std::string>&& protocols,
  GURL&& origin,
  std::string&& additional_headers) {

  auto session = Session::FromPartition(isolate, sessionName);
  auto browser_context = session->browser_context();
  auto websocket = new WebSocket(isolate, wrapper, url);
  websocket->atom_websocket_channel_ = AtomWebSocketChannel::Create(
    browser_context,
    std::move(url),
    std::move(protocols),
    std::move(origin),
    std::move(additional_headers),
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
    .SetMethod("_close", &WebSocket::Close)
    .SetProperty("binaryType", &WebSocket::GetBinaryType, 
      &WebSocket::SetBinaryType)
    .SetProperty("bufferedAmount", &WebSocket::BufferedAmount)
    .SetProperty("extensions", &WebSocket::Extensions)
    .SetProperty("protocol", &WebSocket::Protocol)
    .SetProperty("readyState", &WebSocket::ReadyState)
    .SetProperty("url", &WebSocket::Url);
}


void WebSocket::Send(scoped_refptr<net::IOBufferWithSize> buffer,
  net::WebSocketFrameHeader::OpCodeEnum op_code,
  bool is_last) {
  atom_websocket_channel_->Send(buffer, op_code, is_last);
}

void WebSocket::Close(uint32_t code, const std::string& reason) {
  if (state_ != State::CLOSED) {
    state_ = State::CLOSING;
  }
  atom_websocket_channel_->Close(static_cast<uint16_t>(code), reason);
}

const char* WebSocket::GetBinaryType() const {
  if (binary_type_ == BinaryType::NODE_BUFFER) {
    return "nodebuffer";
  } else {
    return "arraybuffer";
  };
}

void WebSocket::SetBinaryType(const std::string& binary_type) {
  if (binary_type == "nodebuffer") {
    binary_type_ = BinaryType::NODE_BUFFER;
  } else if (binary_type == "arraybuffer") {
    binary_type_ = BinaryType::ARRAY_BUFFER;
  } else {
    isolate()->ThrowException(v8::Exception::Error(mate::StringToV8(
      isolate(), "Unsupported binaryType")));
  }
}

uint32_t WebSocket::BufferedAmount() const {
  return buffered_amount_;
}

const std::string& WebSocket::Extensions() const {
  return extensions_;
}

const std::string& WebSocket::Protocol() const {
  return selected_subprotocol_;
}

WebSocket::State WebSocket::ReadyState() const {
  return state_;
}

GURL WebSocket::Url() const {
  return url_;
}

void WebSocket::OnStartOpeningHandshake(
  std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
  handshake_request_info_ = std::move(request);
}

void WebSocket::OnFinishOpeningHandshake(
  std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  handshake_response_info_ = std::move(response);
  state_ = State::OPEN;
  v8::HandleScope handle_scope(isolate());
  mate::EmitEvent(isolate(), GetWrapper(), "open");
}

void WebSocket::OnAddChannelResponse(
  const std::string& selected_subprotocol,
  const std::string& extensions) {
  selected_subprotocol_ = selected_subprotocol;
  extensions_ = extensions;
}

void WebSocket::OnBufferedAmountUpdate(uint32_t buffered_amount) {
  buffered_amount_ = buffered_amount;
}

void WebSocket::OnDataFrame(bool fin,
  net::WebSocketFrameHeader::OpCodeEnum type,
  scoped_refptr<net::IOBuffer> buffer,
  size_t buffer_size) {

  v8::HandleScope handle_scope(isolate());
  auto data = node::Buffer::Copy(isolate(),
    buffer->data(),
    buffer_size).ToLocalChecked();

  // TODO: implement conversion to ArrayBuffer if needed.
  mate::EmitEvent(isolate(), GetWrapper(), "message", data);
}

void WebSocket::OnClosingHandshake() {
}

void WebSocket::OnDropChannel(bool was_clean, uint32_t code,
  const std::string& reason) {
  state_ = State::CLOSED;
  v8::HandleScope handle_scope(isolate());
  mate::EmitEvent(isolate(), GetWrapper(), "close", code, reason);
}

void WebSocket::OnFailChannel(const std::string& message) {
  v8::HandleScope handle_scope(isolate());
  auto error_object = v8::Exception::Error(mate::StringToV8(isolate(), 
    message));
  mate::EmitEvent(isolate(), GetWrapper(), "error", error_object);
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
