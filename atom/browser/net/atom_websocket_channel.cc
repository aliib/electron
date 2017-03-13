// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/net/atom_websocket_channel.h"

#include "atom/browser/api/atom_api_websocket.h"
#include "atom/browser/atom_browser_context.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/websockets/websocket_event_interface.h"
#include "net/websockets/websocket_handshake_request_info.h"
#include "net/websockets/websocket_handshake_response_info.h"

namespace atom {

class WebSocketEventHandler : public net::WebSocketEventInterface {
 public:
  explicit WebSocketEventHandler(AtomWebSocketChannel* owner) : owner_(owner) {}

  virtual ~WebSocketEventHandler() {}

  virtual void OnCreateURLRequest(net::URLRequest* request) {}

  virtual ChannelState OnAddChannelResponse(
      const std::string& selected_subprotocol,
      const std::string& extensions) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnAddChannelResponse, owner_,
                   selected_subprotocol, extensions));
    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnDataFrame(bool fin,
                                   WebSocketMessageType type,
                                   scoped_refptr<net::IOBuffer> buffer,
                                   size_t buffer_size) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnDataFrame, owner_, fin,
                   static_cast<net::WebSocketFrameHeader::OpCodeEnum>(type),
                   buffer, buffer_size));

    // Avoid potential re-entry issues with SendFlowControl <-> OnDataFrame.
    // SendFlowControl in the next IO cycle.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::DoSendFlowControl, owner_));

    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnFlowControl(int64_t quota) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    // Avoid potential re-entry issues with OnFlowControl <-> SendFrame.
    // Check pending frames in the next IO cycle.
    content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnFlowControl, owner_, quota));
    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnClosingHandshake() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnClosingHandshake, owner_));
    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnDropChannel(bool was_clean,
                                     uint16_t code,
                                     const std::string& reason) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnDropChannel, owner_, was_clean,
                   code, reason));

    return ChannelState::CHANNEL_DELETED;
  }

  virtual ChannelState OnFailChannel(const std::string& message) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnFailChannel, owner_, message));
    return ChannelState::CHANNEL_DELETED;
  }

  virtual ChannelState OnStartOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnStartOpeningHandshake, owner_,
                   base::Passed(std::move(request))));
    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnFinishOpeningHandshake(
      std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&AtomWebSocketChannel::OnFinishOpeningHandshake, owner_,
                   base::Passed(std::move(response))));
    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnSSLCertificateError(
      std::unique_ptr<SSLErrorCallbacks> ssl_error_callbacks,
      const GURL& url,
      const net::SSLInfo& ssl_info,
      bool fatal) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return ChannelState::CHANNEL_ALIVE;
  }

 private:
  AtomWebSocketChannel* owner_;

  WebSocketEventHandler(const WebSocketEventHandler&) = delete;
  WebSocketEventHandler& operator=(const WebSocketEventHandler&) = delete;
};

scoped_refptr<AtomWebSocketChannel> AtomWebSocketChannel::Create(
    AtomBrowserContext* browser_context,
    const GURL& url,
    const std::vector<std::string>& protocols,
    const GURL& origin,
    const std::string& additional_headers,
    api::WebSocket* delegate) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(browser_context);
  DCHECK(!url.is_empty());
  if (!browser_context || url.is_empty()) {
    return nullptr;
  }
  auto request_context_getter = browser_context->url_request_context_getter();
  DCHECK(request_context_getter);
  if (!request_context_getter) {
    return nullptr;
  }

  scoped_refptr<AtomWebSocketChannel> atom_websocket_channel(
      new AtomWebSocketChannel(delegate));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::DoInitialize, atom_websocket_channel,
                 request_context_getter, std::move(url), std::move(protocols),
                 std::move(origin), std::move(additional_headers)));
  return atom_websocket_channel;
}

AtomWebSocketChannel::AtomWebSocketChannel(api::WebSocket* delegate)
    : delegate_(delegate), send_quota_(0), buffered_amount_(0) {}

AtomWebSocketChannel::~AtomWebSocketChannel() {}

void AtomWebSocketChannel::Terminate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_ = nullptr;
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::DoTerminate, this));
}

void AtomWebSocketChannel::DoInitialize(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    const GURL& url,
    const std::vector<std::string>& protocols,
    const GURL& origin,
    const std::string& additional_headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(request_context_getter);

  auto context = request_context_getter->GetURLRequestContext();
  if (!context) {
    // Called after shutdown.
    // DoCancelWithError("Cannot start a request after shutdown.", true);
    return;
  }

  std::unique_ptr<net::WebSocketEventInterface> websocket_events(
      new WebSocketEventHandler(this));

  websocket_channel_.reset(
      new net::WebSocketChannel(std::move(websocket_events), context));

  GURL first_party_for_cookies;
  websocket_channel_->SendAddChannelRequest(url, protocols, url::Origin(origin),
                                            first_party_for_cookies,
                                            additional_headers);

  DoSendFlowControl();
}

void AtomWebSocketChannel::DoTerminate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  websocket_channel_.reset();
}

void AtomWebSocketChannel::Send(scoped_refptr<net::IOBufferWithSize> buffer,
                                net::WebSocketFrameHeader::OpCodeEnum op_code,
                                bool is_last) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                   base::Bind(&AtomWebSocketChannel::DoSend,
                                              this, buffer, op_code, is_last));
}

void AtomWebSocketChannel::Close(uint16_t code, const std::string& reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::DoClose, this, code, reason));
}

void AtomWebSocketChannel::DoSend(scoped_refptr<net::IOBufferWithSize> buffer,
                                  net::WebSocketFrameHeader::OpCodeEnum op_code,
                                  bool is_last) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  buffered_amount_ += buffer->size();
  pending_frames_.push_back(std::make_unique<WebSocketFrame>(
      buffer, buffer->size(), op_code, is_last));
  DoProcessPendingFrames();
}

void AtomWebSocketChannel::DoProcessPendingFrames() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK_GE(send_quota_, 0);

  if (pending_frames_.empty()) {
    return;
  }

  if (send_quota_ == 0) {
    return;
  }

  // Pop front.
  auto frame = std::move(pending_frames_.front());
  pending_frames_.pop_front();

  auto buffer = frame->buffer_;
  auto op_code = frame->op_code_;
  auto fin = frame->fin_;
  auto buffer_size = static_cast<int64_t>(frame->size_);

  // Split the frame if its size is greater than the send quota.
  if (buffer_size > send_quota_) {
    // Force fin to false it cannot be the last frame.
    fin = false;

    // Safe cast as send_quota_ is less than int next_frame_size.
    auto send_quota = static_cast<int>(send_quota_);

    // Build a buffer equal to the send quota.
    scoped_refptr<net::IOBufferWithSize> splitted_buffer =
        new net::IOBufferWithSize(send_quota);
    memcpy(splitted_buffer->data(), buffer->data(), send_quota);

    // Re-push the remaining data on front of pending frames keeping the same
    // fin flag.
    auto remaining_size = buffer_size - send_quota;
    scoped_refptr<net::IOBufferWithSize> remaining_buffer =
        new net::IOBufferWithSize(static_cast<size_t>(remaining_size));
    memcpy(remaining_buffer->data(), buffer->data() + send_quota_,
           remaining_size);

    pending_frames_.push_front(std::make_unique<WebSocketFrame>(
        remaining_buffer, remaining_size, op_code, frame->fin_));

    buffer = splitted_buffer;
    buffer_size = send_quota;
  }

  send_quota_ -= buffer_size;
  buffered_amount_ -= buffer_size;
  auto channel_state =
      websocket_channel_->SendFrame(fin, op_code, buffer, buffer_size);
}

void AtomWebSocketChannel::DoSendFlowControl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  websocket_channel_->SendFlowControl(INT_MAX);
}

void AtomWebSocketChannel::DoClose(uint16_t code, const std::string& reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  websocket_channel_->StartClosingHandshake(code, reason);
}

void AtomWebSocketChannel::OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnStartOpeningHandshake(std::move(request));
  }
}

void AtomWebSocketChannel::OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnFinishOpeningHandshake(std::move(response));
  }
}

void AtomWebSocketChannel::OnAddChannelResponse(
    const std::string& selected_subprotocol,
    const std::string& extensions) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnAddChannelResponse(selected_subprotocol, extensions);
  }
}

void AtomWebSocketChannel::OnDataFrame(
    bool fin,
    net::WebSocketFrameHeader::OpCodeEnum type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnDataFrame(fin, type, buffer, buffer_size);
  }
}

void AtomWebSocketChannel::OnBufferedAmountUpdate(uint32_t buffered_amount) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnBufferedAmountUpdate(buffered_amount);
  }
}

void AtomWebSocketChannel::OnFlowControl(int64_t quota) {
  // Send flow control is entirely managed in the IO thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  send_quota_ = quota;
  DoProcessPendingFrames();
}

void AtomWebSocketChannel::OnClosingHandshake() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnClosingHandshake();
  }
}

void AtomWebSocketChannel::OnDropChannel(bool was_clean,
                                         uint16_t code,
                                         const std::string& reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnDropChannel(was_clean, code, reason);
  }
}

void AtomWebSocketChannel::OnFailChannel(const std::string& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (delegate_) {
    delegate_->OnFailChannel(message);
  }
}

}  // namespace atom
