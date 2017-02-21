// Copyright (c) 2016 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_
#define ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_

#include <base/macros.h>
#include "atom/browser/api/event_emitter.h"

namespace atom {

class AtomURLRequest;

namespace api {

class WebSocket : public mate::EventEmitter<WebSocket>  {
public:
  static mate::WrappableBase* New(mate::Arguments* args);

  static void BuildPrototype(v8::Isolate* isolate,
    v8::Local<v8::FunctionTemplate> prototype);

protected:
  explicit WebSocket(v8::Isolate* isolate, v8::Local<v8::Object> wrapper);
  ~WebSocket() override;

private:
  void Send();
  void Close();

  DISALLOW_COPY_AND_ASSIGN(WebSocket);
};

}  // namespace api

}  // namespace atom

#endif  // ATOM_BROWSER_API_ATOM_API_WEBSOCKET_H_