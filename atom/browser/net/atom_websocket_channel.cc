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
  explicit WebSocketEventHandler(AtomWebSocketChannel* owner)
    : owner_(owner) {
  }

  virtual ~WebSocketEventHandler() override {
  }

  virtual void OnCreateURLRequest(net::URLRequest* request) {
  }

  virtual ChannelState OnAddChannelResponse(
    const std::string& selected_subprotocol,
    const std::string& extensions) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnAddChannelResponse,
        owner_,
        selected_subprotocol,
        extensions));
    return ChannelState::CHANNEL_ALIVE;
  };

  virtual ChannelState OnDataFrame(bool fin,
    WebSocketMessageType type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnDataFrame,
        owner_,
        fin,
        static_cast<net::WebSocketFrameHeader::OpCodeEnum>(type),
        buffer,
        buffer_size));

    return ChannelState::CHANNEL_ALIVE;
  };

  virtual ChannelState OnFlowControl(int64_t quota) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnFlowControl,
        owner_,
        quota));
    return ChannelState::CHANNEL_ALIVE;
  }

  // Called when the remote server has Started the WebSocket Closing
  // Handshake. The client should not attempt to send any more messages after
  // receiving this message. It will be followed by OnDropChannel() when the
  // closing handshake is complete.
  virtual ChannelState OnClosingHandshake() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnClosingHandshake,
        owner_));
    return ChannelState::CHANNEL_ALIVE;
  }

  // Called when the channel has been dropped, either due to a network close, a
  // network error, or a protocol error. This may or may not be preceeded by a
  // call to OnClosingHandshake().
  //
  // Warning: Both the |code| and |reason| are passed through to Javascript, so
  // callers must take care not to provide details that could be useful to
  // attackers attempting to use WebSockets to probe networks.
  //
  // |was_clean| should be true if the closing handshake completed successfully.
  //
  // The channel should not be used again after OnDropChannel() has been
  // called.
  //
  // This method returns a ChannelState for consistency, but all implementations
  // must delete the Channel and return CHANNEL_DELETED.
  virtual ChannelState OnDropChannel(bool was_clean,
    uint16_t code,
    const std::string& reason) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnDropChannel,
        owner_,
        was_clean,
        code,
        reason));

    return ChannelState::CHANNEL_DELETED;
  }

  // Called when the browser fails the channel, as specified in the spec.
  //
  // The channel should not be used again after OnFailChannel() has been
  // called.
  //
  // This method returns a ChannelState for consistency, but all implementations
  // must delete the Channel and return CHANNEL_DELETED.
  virtual ChannelState OnFailChannel(const std::string& message) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnFailChannel,
        owner_,
        message));
    return ChannelState::CHANNEL_DELETED;
  }

  virtual ChannelState OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnStartOpeningHandshake,
        owner_,
        base::Passed(std::move(request))));
    return ChannelState::CHANNEL_ALIVE;
  }

  virtual ChannelState OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(&AtomWebSocketChannel::OnFinishOpeningHandshake,
        owner_,
        base::Passed(std::move(response))));
    return ChannelState::CHANNEL_ALIVE;
  }


  // Called on SSL Certificate Error during the SSL handshake. Should result in
  // a call to either ssl_error_callbacks->ContinueSSLRequest() or
  // ssl_error_callbacks->CancelSSLRequest(). Normally the implementation of
  // this method will delegate to content::SSLManager::OnSSLCertificateError to
  // make the actual decision. The callbacks must not be called after the
  // WebSocketChannel has been destroyed.
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

};

scoped_refptr<AtomWebSocketChannel> AtomWebSocketChannel::Create(
  AtomBrowserContext* browser_context,
  GURL&& url,
  std::vector<std::string>&& protocols,
  GURL&& origin,
  std::string&& additional_headers,
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
    base::Bind(&AtomWebSocketChannel::DoInitialize,
      atom_websocket_channel,
      request_context_getter,
      std::move(url),
      std::move(protocols),
      std::move(origin),
      std::move(additional_headers)
    ));
  return atom_websocket_channel;
}

AtomWebSocketChannel::AtomWebSocketChannel(api::WebSocket* delegate)
  : delegate_(delegate) {
}

AtomWebSocketChannel::~AtomWebSocketChannel() {
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
    //DoCancelWithError("Cannot start a request after shutdown.", true);
    return;
  }

  std::unique_ptr<net::WebSocketEventInterface> websocket_events(
    new WebSocketEventHandler(this));

  websocket_channel_.reset(new net::WebSocketChannel(
    std::move(websocket_events), 
    context));

  GURL first_party_for_cookies;
  websocket_channel_->SendAddChannelRequest(url, protocols, url::Origin(origin),
    first_party_for_cookies, additional_headers);

  // TODO
  websocket_channel_->SendFlowControl(100000);
}

void AtomWebSocketChannel::DoTerminate() {
}

void AtomWebSocketChannel::Send(
  scoped_refptr<net::IOBufferWithSize> buffer,
  net::WebSocketFrameHeader::OpCodeEnum op_code, bool is_last) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
 content::BrowserThread::PostTask(
    content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AtomWebSocketChannel::DoSend, this, buffer, op_code, is_last));
}

void AtomWebSocketChannel::Close(uint16_t code, const std::string& reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
    content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AtomWebSocketChannel::DoClose, this, code, reason));
}

void AtomWebSocketChannel::DoSend(
  scoped_refptr<net::IOBufferWithSize> buffer,
  net::WebSocketFrameHeader::OpCodeEnum op_code, bool is_last) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto channel_state = websocket_channel_->SendFrame(is_last, 
    op_code, 
    buffer,
    buffer->size());
}

void AtomWebSocketChannel::DoClose(uint16_t code, const std::string& reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  websocket_channel_->StartClosingHandshake(code, reason);
}

void AtomWebSocketChannel::OnStartOpeningHandshake(
  std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
  delegate_->OnStartOpeningHandshake(std::move(request));
}

void AtomWebSocketChannel::OnFinishOpeningHandshake(
  std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnFinishOpeningHandshake(std::move(response));
}


void AtomWebSocketChannel::OnAddChannelResponse(
  const std::string& selected_subprotocol,
  const std::string& extensions) {
  delegate_->OnAddChannelResponse(selected_subprotocol, extensions);
}


void AtomWebSocketChannel::OnDataFrame(bool fin,
  net::WebSocketFrameHeader::OpCodeEnum type,
  scoped_refptr<net::IOBuffer> buffer,
  size_t buffer_size) {
  delegate_->OnDataFrame(fin, type, buffer, buffer_size);
}

void AtomWebSocketChannel::OnFlowControl(int64_t quota) {
  delegate_->OnFlowControl(quota);
}

void AtomWebSocketChannel::OnClosingHandshake() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void AtomWebSocketChannel::OnDropChannel(bool was_clean, uint16_t code,
  const std::string& reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  delegate_->OnDropChannel(was_clean, code, reason);
}

void AtomWebSocketChannel::OnFailChannel(const std::string& message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}


}  // namespace atom