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

namespace net {
class WebSocketEventInterface;
class WebSocketChannel;
class URLRequestContext;
struct WebSocketHandshakeResponseInfo;
}

class GURL;
namespace url {
class Origin;
}

namespace atom {
  
class AtomWebSocketChannel : public base::RefCountedThreadSafe<AtomWebSocketChannel>{
public:
  AtomWebSocketChannel(net::URLRequestContext* url_request_context);

  void SendAddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const url::Origin& origin,
    const GURL& first_party_for_cookies,
    const std::string& additional_headers);

private:
  void DoSendAddChannelRequest(
    const GURL& socket_url,
    const std::vector<std::string>& requested_protocols,
    const url::Origin& origin,
    const GURL& first_party_for_cookies,
    const std::string& additional_headers);

  void OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response);

  void InformFinishOpeningHanshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response);
private:
  std::unique_ptr<net::WebSocketChannel> websocket_channel_;
  friend class WebSocketEventHandler;

  DISALLOW_COPY_AND_ASSIGN(AtomWebSocketChannel);
};


}

#endif  // ATOM_BROWSER_NET_ATOM_WEBSOCKET_CHANNEL_H_