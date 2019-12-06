#include "net/quic/raw_quic/raw_quic_api.h"

#include "net/quic/raw_quic/raw_quic.h"

int64_t RAW_QUIC_CALL RawQuicOpen(RawQuicCallbacks callback,
                                  void* opaque,
                                  bool verify) {
  net::RawQuic* raw_quic = new net::RawQuic(callback, opaque, verify);
  return (int64_t)raw_quic;
}

int32_t RAW_QUIC_CALL RawQuicClose(int64_t handle) {
  if (handle == 0) {
    return RAW_QUIC_ERROR_CODE_INVALID_HANDLE;
  }

  net::RawQuic* raw_quic = (net::RawQuic*)handle;
  raw_quic->Close();
  delete raw_quic;

  return RAW_QUIC_ERROR_CODE_SUCCESS;
}

int32_t RAW_QUIC_CALL RawQuicConnect(int64_t handle,
                                     const char* host,
                                     uint16_t port,
                                     int32_t timeout) {
  if (handle == 0) {
    return RAW_QUIC_ERROR_CODE_INVALID_HANDLE;
  }

  net::RawQuic* raw_quic = (net::RawQuic*)handle;
  return raw_quic->Connect(host, port, timeout);
}

int32_t RAW_QUIC_CALL RawQuicSend(int64_t handle,
                                  uint8_t* data,
                                  uint32_t size) {
  if (handle == 0) {
    return RAW_QUIC_ERROR_CODE_INVALID_HANDLE;
  }

  net::RawQuic* raw_quic = (net::RawQuic*)handle;
  return raw_quic->Write(data, size);
}

int32_t RAW_QUIC_CALL RawQuicRecv(int64_t handle,
                                  uint8_t* data,
                                  uint32_t size,
                                  int32_t timeout) {
  if (handle == 0) {
    return RAW_QUIC_ERROR_CODE_INVALID_HANDLE;
  }

  net::RawQuic* raw_quic = (net::RawQuic*)handle;
  return raw_quic->Read(data, size, timeout);
}
