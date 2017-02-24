// Copyright (c) 2016 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_
#define ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_

#include <base/macros.h>
#include "atom/browser/api/event_emitter.h"
#include "net/websockets/websocket_frame.h"

namespace net {
class IOBufferWithSize;
struct WebSocketHandshakeResponseInfo;
} // namespace net

namespace atom {

class AtomWebSocketChannel;

namespace api {

class WebSocket : public mate::EventEmitter<WebSocket>  {
public:
  static mate::WrappableBase* New(mate::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> prototype);

  void OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response);
  void OnDataFrame(bool fin,
    net::WebSocketFrameHeader::OpCodeEnum type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size);
protected:
  explicit WebSocket(v8::Isolate* isolate, v8::Local<v8::Object> wrapper);
  virtual ~WebSocket() override;

private:
  void Send(scoped_refptr<net::IOBufferWithSize> buffer, bool is_last);
  void Close();
  void Pin();
  void Unpin();

  scoped_refptr<AtomWebSocketChannel> atom_websocket_channel_;
  v8::Global<v8::Object> wrapper_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_