// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_RAW_QUIC_RAW_QUIC_DEFINE_H_
#define NET_QUIC_RAW_QUIC_RAW_QUIC_DEFINE_H_

#ifdef WIN32
#ifdef RAW_QUIC_SHARED_LIBRARY
#ifdef RAW_QUIC_EXPORTS
#define RAW_QUIC_API __declspec(dllexport)
#else
#define RAW_QUIC_API __declspec(dllimport)
#endif
#else
#define RAW_QUIC_API
#endif
#define RAW_QUIC_CALL __cdecl
#define RAW_QUIC_CALLBACK __cdecl
#else
#ifdef RAW_QUIC_EXPORTS
#define RAW_QUIC_API __attribute__((visibility("default")))
#else
#define RAW_QUIC_API
#endif
#define RAW_QUIC_CALL
#define RAW_QUIC_CALLBACK
#endif

// C99 support required.
#include <stdint.h>
#include <stdbool.h>

typedef enum RawQuicErrorCode {
  RAW_QUIC_ERROR_CODE_SUCCESS               = 0,
  RAW_QUIC_ERROR_CODE_INVALID_PARAM         = -1,
  RAW_QUIC_ERROR_CODE_INVALID_STATE         = -2,
  RAW_QUIC_ERROR_CODE_NULL_POINTER          = -3,
  RAW_QUIC_ERROR_CODE_SOCKET_ERROR          = -4,
  RAW_QUIC_ERROR_CODE_RESOLVE_FAILED        = -5,
  RAW_QUIC_ERROR_CODE_BUFFER_OVERFLOWED     = -6,
  RAW_QUIC_ERROR_CODE_STREAM_FIN            = -7,
  RAW_QUIC_ERROR_CODE_STREAM_RESET          = -8,
  RAW_QUIC_ERROR_CODE_NET_ERROR             = -9,
  RAW_QUIC_ERROR_CODE_QUIC_ERROR            = -10,
  RAW_QUIC_ERROR_CODE_TIMEOUT               = -11,
  RAW_QUIC_ERROR_CODE_UNKNOWN               = -12,
  RAW_QUIC_ERROR_CODE_INVALID_HANDLE        = -13,
  RAW_QUIC_ERROR_CODE_COUNT
} RawQuicErrorCode;

typedef struct RawQuicError {
  RawQuicErrorCode error;
  int32_t net_error;
  int32_t quic_error;
} RawQuicError;

typedef void(RAW_QUIC_CALLBACK* ConnectCallback)(void* opaque,
                                                 RawQuicError* error);

typedef void(RAW_QUIC_CALLBACK* ErrorCallback)(void* opaque,
                                               RawQuicError* error);

typedef void(RAW_QUIC_CALLBACK* CanReadCallback)(uint32_t size);

typedef struct RawQuicCallbacks {
  ConnectCallback connect_callback;
  ErrorCallback error_callback;
  CanReadCallback can_read_callback;
} RawQuicCallbacks;

#endif  // NET_QUIC_RAW_QUIC_RAW_QUIC_DEFINE_H_
