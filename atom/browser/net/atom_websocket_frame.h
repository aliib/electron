// Copyright (c) 2017 GitHub, Inc.
// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_NET_ATOM_WEBSOCKET_FRAME_H_
#define ATOM_BROWSER_NET_ATOM_WEBSOCKET_FRAME_H_

#include "base/memory/ref_counted.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_frame.h"

namespace atom {

  struct WebSocketFrame {
    WebSocketFrame(scoped_refptr<net::IOBuffer> buffer,
      size_t size,
      net::WebSocketFrameHeader::OpCodeEnum op_code,
      bool fin)
      : buffer_(buffer)
      , size_(size)
      , op_code_(op_code)
      , fin_(fin) {
    }

    WebSocketFrame(const WebSocketFrame&) = default;
    WebSocketFrame& operator=(const WebSocketFrame&) = default;
    WebSocketFrame(WebSocketFrame&&) = default;
    WebSocketFrame& operator=(WebSocketFrame&&) = default;
    ~WebSocketFrame() = default;

    scoped_refptr<net::IOBuffer> buffer_;
    size_t size_;
    net::WebSocketFrameHeader::OpCodeEnum op_code_;
    bool fin_;
  };

}  // namespace atom

#endif  // ATOM_BROWSER_NET_ATOM_WEBSOCKET_FRAME_H_