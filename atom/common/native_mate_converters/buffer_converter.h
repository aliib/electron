// Copyright (c) 2017 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#ifndef ATOM_COMMON_NATIVE_MATE_CONVERTERS_BUFFER_CONVERTER_H_
#define ATOM_COMMON_NATIVE_MATE_CONVERTERS_BUFFER_CONVERTER_H_

#include "base/memory/ref_counted.h"
#include "native_mate/converter.h"
#include "net/base/io_buffer.h"

namespace mate {

template <>
struct Converter<scoped_refptr<const net::IOBufferWithSize>> {
  static v8::Local<v8::Value> ToV8(
      v8::Isolate* isolate,
      scoped_refptr<const net::IOBufferWithSize> buffer);

  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     scoped_refptr<const net::IOBufferWithSize>* out);
};

template <>
struct Converter<scoped_refptr<net::IOBufferWithSize>> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   scoped_refptr<net::IOBufferWithSize> buffer);

  static bool FromV8(v8::Isolate* isolate,
                     v8::Local<v8::Value> val,
                     scoped_refptr<net::IOBufferWithSize>* out);
};

}  // namespace mate

#endif  // ATOM_COMMON_NATIVE_MATE_CONVERTERS_BUFFER_CONVERTER_H_
