// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/common/native_mate_converters/buffer_converter.h"
#include "atom/common/node_includes.h"
#include "net/base/io_buffer.h"

namespace mate {

v8::Local<v8::Value>
Converter<scoped_refptr<const net::IOBufferWithSize>>::ToV8(
    v8::Isolate* isolate,
    scoped_refptr<const net::IOBufferWithSize> buffer) {
  return node::Buffer::Copy(isolate, buffer->data(), buffer->size())
      .ToLocalChecked();
}

bool Converter<scoped_refptr<const net::IOBufferWithSize>>::FromV8(
    v8::Isolate* isolate,
    v8::Local<v8::Value> val,
    scoped_refptr<const net::IOBufferWithSize>* out) {
  scoped_refptr<net::IOBufferWithSize> tmp;
  auto result = Converter<scoped_refptr<net::IOBufferWithSize>>::FromV8(
      isolate, val, &tmp);
  *out = tmp;
  return result;
}

v8::Local<v8::Value> Converter<scoped_refptr<net::IOBufferWithSize>>::ToV8(
    v8::Isolate* isolate,
    scoped_refptr<net::IOBufferWithSize> buffer) {
  return Converter<scoped_refptr<const net::IOBufferWithSize>>::ToV8(isolate,
                                                                     buffer);
}

bool Converter<scoped_refptr<net::IOBufferWithSize>>::FromV8(
    v8::Isolate* isolate,
    v8::Local<v8::Value> val,
    scoped_refptr<net::IOBufferWithSize>* out) {
  auto size = node::Buffer::Length(val);

  if (size == 0) {
    // Support conversion from empty buffer. A use case is
    // a GET request without body.
    // Since zero-sized IOBuffer(s) are not supported, we set the
    // out pointer to null.
    *out = nullptr;
    return true;
  }
  auto data = node::Buffer::Data(val);
  if (!data) {
    // This is an error as size is positive but data is null.
    return false;
  }

  *out = new net::IOBufferWithSize(size);
  // We do a deep copy. We could have used Buffer's internal memory
  // but that is much more complicated to be properly handled.
  memcpy((*out)->data(), data, size);
  return true;
}

}  // namespace atom