// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_RAW_QUIC_RAW_QUIC_API_H_
#define NET_QUIC_RAW_QUIC_RAW_QUIC_API_H_

#include "raw_quic_define.h"

#ifdef __cplusplus
extern "C" {
#endif

RAW_QUIC_API int64_t RAW_QUIC_CALL RawQuicOpen(RawQuicCallbacks callback,
                                               void* opaque,
                                               bool verify);

RAW_QUIC_API int32_t RAW_QUIC_CALL RawQuicClose(int64_t handle);

RAW_QUIC_API int32_t RAW_QUIC_CALL RawQuicConnect(int64_t handle,
                                                  const char* host,
                                                  uint16_t port,
                                                  int32_t timeout);

RAW_QUIC_API int32_t RAW_QUIC_CALL RawQuicSend(int64_t handle,
                                               uint8_t* data,
                                               uint32_t size);

RAW_QUIC_API int32_t RAW_QUIC_CALL RawQuicRecv(int64_t handle,
                                               uint8_t* data,
                                               uint32_t size,
                                               int32_t timeout);

#ifdef __cplusplus
}
#endif

#endif  // NET_QUIC_RAW_QUIC_RAW_QUIC_API_H_
