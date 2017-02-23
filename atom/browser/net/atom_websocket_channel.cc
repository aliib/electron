// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/net/atom_websocket_channel.h"
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
  WebSocketEventHandler(AtomWebSocketChannel* owner)
    : owner_(owner) {
  }

  // Called when a URLRequest is created for handshaking.
  virtual void OnCreateURLRequest(net::URLRequest* request) {
  }

  // Called in response to an AddChannelRequest. This means that a response has
  // been received from the remote server.
  virtual ChannelState OnAddChannelResponse(
    const std::string& selected_subprotocol,
    const std::string& extensions) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return ChannelState::CHANNEL_ALIVE;
  };


  // Called when a data frame has been received from the remote host and needs
  // to be forwarded to the renderer process.
  virtual ChannelState OnDataFrame(
    bool fin,
    WebSocketMessageType type,
    const std::vector<char>& data) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return ChannelState::CHANNEL_ALIVE;
  }

  // Called when a data frame has been received from the remote host and needs
  // to be forwarded to the renderer process.
  virtual ChannelState OnDataFrame(bool fin,
    WebSocketMessageType type,
    scoped_refptr<net::IOBuffer> buffer,
    size_t buffer_size) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return ChannelState::CHANNEL_ALIVE;
  };

  // Called to provide more send quota for this channel to the renderer
  // process. Currently the quota units are always bytes of message body
  // data. In future it might depend on the type of multiplexing in use.
  virtual ChannelState OnFlowControl(int64_t quota) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return ChannelState::CHANNEL_ALIVE;
  }

  // Called when the remote server has Started the WebSocket Closing
  // Handshake. The client should not attempt to send any more messages after
  // receiving this message. It will be followed by OnDropChannel() when the
  // closing handshake is complete.
  virtual ChannelState OnClosingHandshake() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
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
    return ChannelState::CHANNEL_DELETED;
  }

  // Called when the browser starts the WebSocket Opening Handshake.
  virtual ChannelState OnStartOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeRequestInfo> request) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    return ChannelState::CHANNEL_ALIVE;
  }

  // Called when the browser finishes the WebSocket Opening Handshake.
  virtual ChannelState OnFinishOpeningHandshake(
    std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    owner_->OnFinishOpeningHandshake(std::move(response));
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
  std::vector<std::string>&& protocols) {
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
    new AtomWebSocketChannel());

  content::BrowserThread::PostTask(
    content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AtomWebSocketChannel::DoInitialize,
      atom_websocket_channel,
      request_context_getter,
      std::move(url),
      std::move(protocols)));
  return atom_websocket_channel;
}

AtomWebSocketChannel::AtomWebSocketChannel() {
}

AtomWebSocketChannel::~AtomWebSocketChannel() {
}

void AtomWebSocketChannel::DoInitialize(
  scoped_refptr<net::URLRequestContextGetter> request_context_getter,
  const GURL& url,
  const std::vector<std::string>& protocols) {

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

  websocket_channel_.reset(new net::WebSocketChannel(std::move(websocket_events), context));

  url::Origin origin;
  GURL first_party_for_cookies;
  std::string additional_headers;
  websocket_channel_->SendAddChannelRequest(url, protocols, origin,
    first_party_for_cookies, additional_headers);
}

void AtomWebSocketChannel::DoTerminate() {
}

void AtomWebSocketChannel::Send(
  scoped_refptr<net::IOBufferWithSize> buffer, bool is_last) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
 content::BrowserThread::PostTask(
    content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AtomWebSocketChannel::DoSend, this, buffer, is_last));
}


void AtomWebSocketChannel::DoSend(
  scoped_refptr<net::IOBufferWithSize> buffer, bool is_last) {
  websocket_channel_->SendFrame(is_last, 
    net::WebSocketFrameHeader::OpCodeEnum::kOpCodeText, 
    buffer,
    buffer->size());
}

void AtomWebSocketChannel::OnFinishOpeningHandshake(
  std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
    base::Bind(&AtomWebSocketChannel::InformFinishOpeningHanshake,
               this,
               base::Passed(std::move(response))));
}

void AtomWebSocketChannel::InformFinishOpeningHanshake(
  std::unique_ptr<net::WebSocketHandshakeResponseInfo> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}





}  // namespace atom