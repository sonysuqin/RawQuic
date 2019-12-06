// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_RAW_QUIC_RAW_QUIC_H_
#define NET_QUIC_RAW_QUIC_RAW_QUIC_H_

#include <future>
#include <memory>
#include <mutex>

#include "net/base/address_list.h"
#include "net/quic/raw_quic/raw_quic_define.h"
#include "net/quic/raw_quic/raw_quic_session.h"
#include "net/quic/raw_quic/streambuf/streambuf.hpp"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/core/quic_packets.h"
#include "net/third_party/quiche/src/quic/core/quic_server_id.h"
#include "net/third_party/quiche/src/quic/quic_transport/quic_transport_stream.h"

namespace net {

class RawQuicContext;

typedef enum RawQuicStatus {
  RAW_QUIC_STATUS_IDLE = 0,
  RAW_QUIC_STATUS_CONNECTING,
  RAW_QUIC_STATUS_CONNECTED,
  RAW_QUIC_STATUS_CLOSING,
  RAW_QUIC_STATUS_CLOSED,
  RAW_QUIC_STATUS_COUNT
} RawQuicStatus;

typedef std::promise<int32_t> IntPromise;
typedef std::shared_ptr<IntPromise> IntPromisePtr;
typedef std::shared_future<int32_t> IntFuture;

///////////////////////////////////RawQuicStreamVisitor///////////////////////////////////////
class RawQuicStreamVisitor : public quic::QuicTransportStream::Visitor {
 public:
  class DataDelegate {
   public:
    virtual ~DataDelegate() {}
    virtual void OnCanRead() = 0;
    virtual void OnFinRead() = 0;
    virtual void OnCanWrite() = 0;
  };

  RawQuicStreamVisitor(DataDelegate* data_delegate);
  void OnCanRead() override;
  void OnFinRead() override;
  void OnCanWrite() override;

 private:
  DataDelegate* data_delegate_ = nullptr;
};

/////////////////////////////////////RawQuic/////////////////////////////////////
class RawQuic : public quic::QuicTransportClientSession::ClientVisitor,
                public quic::QuicSession::Visitor,
                public net::RawQuicSession::Visitor,
                public net::RawQuicStreamVisitor::DataDelegate {
 public:
  RawQuic(RawQuicCallbacks callback, void* opaque, bool verify);
  ~RawQuic() override;

 public:
  int32_t Connect(const char* host, uint16_t port, int32_t timeout);

  void Close();

  int32_t Write(uint8_t* data, uint32_t size);

  int32_t Read(uint8_t* data, uint32_t size, int32_t timeout);

  void SetSendBufferSize(uint32_t size);

  void SetRecvBufferSize(uint32_t size);

 protected:
  void DoConnect(const std::string& host, uint16_t port, IntPromisePtr promise);

  void DoClose(IntPromisePtr promise);

  void DoWrite(uint8_t* data, uint32_t size);

  void DoSetSendBufferSize(uint32_t size);

  void DoSetRecvBufferSize(uint32_t size);

  RawQuicContext* GetContext();

  RawQuicError Resolve(const std::string& host, net::AddressList* addrlist);

  RawQuicError CreateSession(const net::IPEndPoint& dest);

  RawQuicError ConfigureSocket(DatagramClientSocket* socket,
                               const net::IPEndPoint& dest);

  std::unique_ptr<quic::QuicConnection> CreateConnection(
      DatagramClientSocket* socket,
      const net::IPEndPoint& dest);

  quic::ParsedQuicVersionVector GetVersions();

  quic::QuicConfig DefaultQuicConfig();

  void FlushWriteBuffer();

  void FillReadBuffer();

  void ReportError(RawQuicError* error);

  void OnClosed(RawQuicError* error);

  // quic::QuicTransportClientSession::ClientVisitor
  void OnIncomingBidirectionalStreamAvailable() override;

  void OnIncomingUnidirectionalStreamAvailable() override;

  // quic::QuicSession::Visitor
  void OnConnectionClosed(quic::QuicConnectionId server_connection_id,
                          quic::QuicErrorCode error,
                          const std::string& error_details,
                          quic::ConnectionCloseSource source) override;

  void OnWriteBlocked(
      quic::QuicBlockedWriterInterface* blocked_writer) override;

  void OnRstStreamReceived(const quic::QuicRstStreamFrame& frame) override;

  void OnStopSendingReceived(const quic::QuicStopSendingFrame& frame) override;

  // net::RawQuicSession::Visitor
  void OnConnectionOpened() override;

  // net::RawQuicStreamVisitor
  void OnCanRead() override;

  void OnFinRead() override;

  void OnCanWrite() override;

 private:
  // Application callback.
  RawQuicCallbacks callback_;
  void* opaque_ = nullptr;

  // Status.
  bool verify_ = true;
  bool can_write_ = false;
  std::atomic<int32_t> status_;
  IntPromisePtr connect_promise_;

  // Endpoint.
  std::string host_;
  uint16_t port_ = 0;
  quic::QuicServerId server_id_;

  // QUIC.
  std::unique_ptr<quic::QuicTransportClientSession> session_;
  // Stream owned by QuicSession, only one supported now.
  quic::QuicTransportStream* stream_ = nullptr;

  // Send buffer.
  uint32_t send_buffer_size_ = 0;
  uint32_t buffered_write_data_size_ = 0;
  std::queue<quic::QuicData> write_queue_;

  // Recv buffer.
  uint32_t recv_buffer_size_ = 0;
  std::mutex read_mutex_;
  std::condition_variable read_cond_;
  boost::asio::streambuf read_buffer_;
  std::unique_ptr<uint8_t[]> temp_read_buffer_;
  std::istream read_istream_;
  std::ostream read_ostream_;
};

}  // namespace net

#endif  // NET_QUIC_RAW_QUIC_RAW_QUIC_H_
