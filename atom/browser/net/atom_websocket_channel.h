// Copyright (c) 2017 GitHub, Inc.
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_NET_ATOM_WEBSOCKET_CHANNEL_H_
#define ATOM_BROWSER_NET_ATOM_WEBSOCKET_CHANNEL_H_

#include <memory>
#include <vector>
#include <string>
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/websockets/websocket_channel.h"
#include "net/websockets/websocket_frame.h"

namespace net {
class WebSocketEventInterface;
class URLRequestContext;
struct WebSocketHandshakeResponseInfo;
}

class GURL;
namespace url {
class Origin;
}

namespace atom {

namespace api {
class WebSocket;
} // namespace api

class AtomBrowserContext;

class AtomWebSocketChannel : 
  public base::RefCountedThreadSafe<AtomWebSocketChannel> {

public:
  static scoped_refptr<AtomWebSocketChannel> Create(
    AtomBrowserContext* browser_context,
    GURL&& url,
    std::vector<std::string>&& protocols,
    GURL&& origin,
    std::string&& additional_headers,
    api::WebSocket* delegate);

  void Send(scoped_refptr<net::IOBufferWithSize> buffer,
    net::WebSocketFrameHeader::OpCodeEnum op_code, bool is_last);
  void Close(uint16_t code, const std::string& reason);

private:
  friend class base::RefCountedThreadSafe<AtomWebSocketChannel>;
  AtomWebSocketChannel(api::WebSocket* delegate);
  ~AtomWebSocketChannel();

  void DoInitialize(scoped_refptr<net::URLRequestContextGetter>,
    const GURL& url,
    const std::vector<std::string>& protocols,
    const GURL& origin,
    const std::string& additional_headers);
  void DoTerminate();

  void DoSend(scoped_refptr<net::IOBufferWithSize> buffer,
    net::WebSocketFrameHeader::OpCodeEnum op_code, bool is_last);
  void DoClose(uint16_t code, const std::string& reason);

  void OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response);
  void OnDataFrame(bool fin,
    net::WebSocketFrameHeader::OpCodeEnum type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size);
  void OnDropChannel(bool was_clean, uint16_t code, const std::string& reason);
private:
  api::WebSocket* delegate_;
  std::unique_ptr<net::WebSocketChannel> websocket_channel_;
  friend class WebSocketEventHandler;

  DISALLOW_COPY_AND_ASSIGN(AtomWebSocketChannel);
};


}

#endif  // ATOM_BROWSER_NET_ATOM_WEBSOCKET_CHANNEL_H_