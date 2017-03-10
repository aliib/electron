// Copyright (c) 2016 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_
#define ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_

#include <queue>
#include <string>
#include <base/macros.h>
#include "atom/browser/api/event_emitter.h"
#include "atom/browser/net/atom_websocket_frame.h"
#include "net/websockets/websocket_frame.h"
#include "url/gurl.h"


namespace net {
class IOBufferWithSize;
struct WebSocketHandshakeRequestInfo;
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

  void OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request);
  void OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response);

  void OnAddChannelResponse(
    const std::string& selected_subprotocol,
    const std::string& extensions);
  void OnBufferedAmountUpdate(uint32_t buffered_amount);
  void OnDataFrame(bool fin,
    net::WebSocketFrameHeader::OpCodeEnum frame_op_code,
    scoped_refptr<net::IOBuffer> frame_buffer,
    size_t frame_size);
  void OnClosingHandshake();
  void OnDropChannel(bool was_clean, uint32_t code, const std::string& reason);
  void OnFailChannel(const std::string& message);

private:
  enum class State {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
  };

  enum class BinaryType {
    NODE_BUFFER,
    ARRAY_BUFFER
  };

  friend struct mate::Converter<atom::api::WebSocket::State>;

  static mate::WrappableBase* NewInternal(v8::Isolate* isolate,
    v8::Local<v8::Object> wrapper,
    std::string&& sessionName,
    GURL&& url,
    std::vector<std::string>&& protocols,
    GURL&& origin,
    std::string&& additional_headers);
  explicit WebSocket(v8::Isolate* isolate, v8::Local<v8::Object> wrapper,
    const GURL& url);
  virtual ~WebSocket() override;

  void Send(scoped_refptr<net::IOBufferWithSize> buffer,
    net::WebSocketFrameHeader::OpCodeEnum op_code,
    bool is_last);
  void Close(uint32_t code, const std::string& reason);
  const char* GetBinaryType() const;
  void SetBinaryType(const std::string& binaryType);
  uint32_t BufferedAmount() const;
  const std::string& Extensions() const;
  const std::string& Protocol() const;
  State ReadyState() const;
  GURL Url() const;
  void Pin();
  void Unpin();

  scoped_refptr<AtomWebSocketChannel> atom_websocket_channel_;
  v8::Global<v8::Object> wrapper_;
  std::unique_ptr<net::WebSocketHandshakeRequestInfo> handshake_request_info_;
  std::unique_ptr<net::WebSocketHandshakeResponseInfo> handshake_response_info_;
  BinaryType binary_type_;
  uint32_t buffered_amount_;
  std::string extensions_;
  std::string selected_subprotocol_;
  State state_;
  GURL url_;

  std::vector<WebSocketFrame> pending_frames_;

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_