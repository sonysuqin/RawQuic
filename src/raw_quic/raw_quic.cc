// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"
#include "net/dns/host_resolver_proc.h"
#include "net/quic/address_utils.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/quic/raw_quic/raw_quic.h"
#include "net/quic/raw_quic/raw_quic_context.h"
#include "net/socket/udp_client_socket.h"
#include "net/third_party/quiche/src/quic/core/quic_utils.h"
#include "net/third_party/quiche/src/quic/core/quic_versions.h"
#include "url/gurl.h"

namespace net {

namespace {
const int32_t kQuicSocketReceiveBufferSize = 1024 * 1024;  // 1MB
const int32_t kMaxIdleNetworkTimeout = 30;
const int32_t kDefaultIdleNetworkTimeout = 30;
const int32_t kSetMaxTimeBeforeCryptoHandshake = 10;
const int32_t kSetMaxIdleTimeBeforeCryptoHandshake = 5;
const int32_t kMinSendBufferSize = 8 * 1024;
const int32_t kMinRecvBufferSize = 8 * 1024;
const int32_t kDefaultSendBufferSize = 512 * 1024;
const int32_t kDefaultRecvBufferSize = 512 * 1024;
const int32_t kReadOnceSize = 32 * 1024;
}  // namespace

///////////////////////////////////RawQuicStreamVisitor///////////////////////////////////////
RawQuicStreamVisitor::RawQuicStreamVisitor(DataDelegate* data_delegate) {
  data_delegate_ = data_delegate;
}

void RawQuicStreamVisitor::OnCanRead() {
  if (data_delegate_) {
    data_delegate_->OnCanRead();
  }
}

void RawQuicStreamVisitor::OnFinRead() {
  if (data_delegate_) {
    data_delegate_->OnFinRead();
  }
}

void RawQuicStreamVisitor::OnCanWrite() {
  if (data_delegate_) {
    data_delegate_->OnCanWrite();
  }
}

////////////////////////////////////RawQuic//////////////////////////////////////
RawQuic::RawQuic(RawQuicCallbacks callback, void* opaque, bool verify)
    : callback_(callback),
      opaque_(opaque),
      verify_(verify),
      status_(RAW_QUIC_STATUS_IDLE),
      send_buffer_size_(kDefaultSendBufferSize),
      recv_buffer_size_(kDefaultRecvBufferSize),
      temp_read_buffer_(new uint8_t[kReadOnceSize]),
      read_istream_(&read_buffer_),
      read_ostream_(&read_buffer_) {}

RawQuic::~RawQuic() {}

int32_t RawQuic::Connect(const char* host,
                         uint16_t port,
                         const char* path,
                         int32_t timeout) {
  int32_t ret = RAW_QUIC_ERROR_CODE_SUCCESS;
  do {
    if (host == nullptr || port == 0) {
      ret = RAW_QUIC_ERROR_CODE_INVALID_PARAM;
      break;
    }

    int32_t status = status_.load();
    if (status != RAW_QUIC_STATUS_IDLE && status != RAW_QUIC_STATUS_CLOSED) {
      ret = RAW_QUIC_ERROR_CODE_INVALID_STATE;
      break;
    }

    IntPromisePtr promise;
    if (timeout != 0) {
      promise.reset(new IntPromise);
    }

    GetContext()->Post(base::Bind(
        &RawQuic::DoConnect, base::Unretained(this), std::string(host), port,
        path == NULL ? "" : std::string(path), promise));

    if (promise == NULL) {
      break;
    }

    IntFuture future = promise->get_future();
    if (timeout < 0) {
      ret = future.get();
      break;
    }

    std::future_status wait_ret = future.wait_until(
        std::chrono::system_clock::now() + std::chrono::milliseconds(timeout));
    if (wait_ret == std::future_status::ready) {
      ret = future.get();
      break;
    }

    ret = RAW_QUIC_ERROR_CODE_TIMEOUT;
  } while (0);
  return ret;
}

void RawQuic::Close() {
  IntPromisePtr promise(new IntPromise);

  GetContext()->Post(
      base::Bind(&RawQuic::DoClose, base::Unretained(this), promise));

  IntFuture future = promise->get_future();
  future.get();
}

int32_t RawQuic::Write(uint8_t* data, uint32_t size) {
  int32_t ret = RAW_QUIC_ERROR_CODE_SUCCESS;
  do {
    if (data == nullptr || size == 0) {
      ret = RAW_QUIC_ERROR_CODE_INVALID_PARAM;
      break;
    }

    int32_t status = status_.load();
    if (status != RAW_QUIC_STATUS_CONNECTED) {
      ret = RAW_QUIC_ERROR_CODE_INVALID_STATE;
      break;
    }

    std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
    memcpy(buffer.get(), data, size);
    ret = size;

    GetContext()->Post(base::Bind(&RawQuic::DoWrite, base::Unretained(this),
                                  buffer.release(), size));
  } while (0);
  return ret;
}

int32_t RawQuic::Read(uint8_t* data, uint32_t size, int32_t timeout) {
  int32_t ret = RAW_QUIC_ERROR_CODE_SUCCESS;
  do {
    if (data == nullptr || size == 0) {
      ret = RAW_QUIC_ERROR_CODE_INVALID_PARAM;
      break;
    }

    int32_t status = status_.load();
    if (status != RAW_QUIC_STATUS_CONNECTED) {
      ret = RAW_QUIC_ERROR_CODE_INVALID_STATE;
      break;
    }

    std::unique_lock<std::mutex> lock(read_mutex_);
    bool wait = true;
    while (read_buffer_.size() == 0) {
      // Try filling read buffer once, worker thread will gain
      // lock after current thread called wait or run out.
      GetContext()->Post(
          base::Bind(&RawQuic::OnCanRead, base::Unretained(this)));
      if (timeout > 0) {
        read_cond_.wait_until(lock, std::chrono::system_clock::now() +
                                        std::chrono::milliseconds(timeout));
        break;
      } else if (timeout < 0) {
        read_cond_.wait(lock);
      } else {
        wait = false;
        break;
      }
    }

    uint32_t read_len = std::min<uint32_t>(size, read_buffer_.size());
    if (read_len == 0) {
      ret = wait ? RAW_QUIC_ERROR_CODE_TIMEOUT : RAW_QUIC_ERROR_CODE_EAGAIN;
      break;
    }

    read_istream_.read((char*)data, read_len);
    ret = read_len;
  } while (0);

  return ret;
}

int32_t RawQuic::GetRecvBufferDataSize() {
  std::unique_lock<std::mutex> lock(read_mutex_);
  return (int32_t)read_buffer_.size();
}

void RawQuic::SetSendBufferSize(uint32_t size) {
  GetContext()->Post(
      base::Bind(&RawQuic::DoSetSendBufferSize, base::Unretained(this), size));
}

uint32_t RawQuic::GetSendBufferSize() {
  return send_buffer_size_;
}

void RawQuic::SetRecvBufferSize(uint32_t size) {
  GetContext()->Post(
      base::Bind(&RawQuic::DoSetRecvBufferSize, base::Unretained(this), size));
}

void RawQuic::DoConnect(const std::string& host,
                        uint16_t port,
                        const std::string& path,
                        IntPromisePtr promise) {
  RawQuicError ret = {RAW_QUIC_ERROR_CODE_SUCCESS, 0, 0};
  do {
    host_ = host;
    port_ = port;
    path_ = path;
    std::string url = base::StringPrintf(
        "quic-transport://%s:%d/%s", host_.c_str(), (int)port, path_.c_str());
    url_ = GURL(url);
    status_.store(RAW_QUIC_STATUS_CONNECTING);

    net::AddressList address_list;
    ret = Resolve(host_, &address_list);
    if (ret.error != RAW_QUIC_ERROR_CODE_SUCCESS) {
      break;
    }

    if (address_list.empty()) {
      ret.error = RAW_QUIC_ERROR_CODE_RESOLVE_FAILED;
      break;
    }

    net::IPEndPoint endpoint = *address_list.begin();
    net::IPAddress addr = endpoint.address();
    net::IPEndPoint dest(addr, port);

    ret = CreateSession(dest);
    if (ret.error != RAW_QUIC_ERROR_CODE_SUCCESS) {
      break;
    }
  } while (0);

  if (ret.error != RAW_QUIC_ERROR_CODE_SUCCESS) {
    status_.store(RAW_QUIC_STATUS_IDLE);
    if (promise != nullptr) {
      promise->set_value(ret.error);
    } else if (callback_.connect_callback != nullptr) {
      callback_.connect_callback(this, &ret, opaque_);
    }
  } else {
    connect_promise_ = promise;
  }
}

void RawQuic::DoClose(IntPromisePtr promise) {
  if (session_ != nullptr) {
    quic::QuicConnection* connection = session_->connection();
    if (connection != nullptr) {
      status_.store(RAW_QUIC_STATUS_CLOSING);
      connection->CloseConnection(
          quic::QUIC_NO_ERROR, "Client shutdown.",
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    }
    session_ = nullptr;
  }

  status_.store(RAW_QUIC_STATUS_CLOSED);
  if (promise != nullptr) {
    promise->set_value(0);
  }
}

void RawQuic::DoWrite(uint8_t* data, uint32_t size) {
  if (buffered_write_data_size_ + size >= send_buffer_size_) {
    LOG(ERROR) << "Send buffer overflow.";
    if (callback_.error_callback != nullptr) {
      RawQuicError ret = {RAW_QUIC_ERROR_CODE_BUFFER_OVERFLOWED, 0, 0};
      callback_.error_callback(this, &ret, opaque_);
    }
    if (data != nullptr) {
      delete [] data;
    }
    return;
  }

  write_queue_.emplace((char*)data, size, true);
  buffered_write_data_size_ += size;

  if (stream_ != nullptr && stream_->visitor() != nullptr) {
    stream_->visitor()->OnCanWrite();
  }
}

void RawQuic::DoSetSendBufferSize(uint32_t size) {
  if (size < kMinSendBufferSize) {
    size = kMinSendBufferSize;
  }
  send_buffer_size_ = size;
}

void RawQuic::DoSetRecvBufferSize(uint32_t size) {
  if (size < kMinRecvBufferSize) {
    size = kMinRecvBufferSize;
  }
  recv_buffer_size_ = size;
}

RawQuicContext* RawQuic::GetContext() {
  return RawQuicContext::GetInstance();
}

RawQuicError RawQuic::Resolve(const std::string& host,
                              net::AddressList* addrlist) {
  RawQuicError ret = {RAW_QUIC_ERROR_CODE_SUCCESS, 0, 0};
  int32_t os_error = 0;
  ret.net_error = net::SystemHostResolverCall(
      host, net::ADDRESS_FAMILY_UNSPECIFIED, 0, addrlist, &os_error);
  if (ret.net_error != net::OK || os_error != 0) {
    ret.error = RAW_QUIC_ERROR_CODE_RESOLVE_FAILED;
    LOG(ERROR) << "Resolve " << host << " failed, error:" << os_error;
  }
  return ret;
}

RawQuicError RawQuic::CreateSession(const net::IPEndPoint& dest) {
  RawQuicError ret = {RAW_QUIC_ERROR_CODE_SUCCESS, 0, 0};

  auto socket = std::unique_ptr<net::DatagramClientSocket>(
      new net::UDPClientSocket(net::DatagramSocket::DEFAULT_BIND,
                               GetContext()->GetNetLogWithSource()->net_log(),
                               GetContext()->GetNetLogWithSource()->source()));
  ret = ConfigureSocket(socket.get(), dest);
  if (ret.error != RAW_QUIC_ERROR_CODE_SUCCESS) {
    return ret;
  }

  auto connection = CreateConnection(socket.get(), dest);

  auto crypto_config = std::make_unique<quic::QuicCryptoClientConfig>(
      GetContext()->CreateProofVerifier(host_, verify_));

  std::string url = std::string("https://") + host_;
  GURL origin_url(url);
  url::Origin origin = url::Origin::Create(origin_url);

  session_ = std::make_unique<RawQuicSession>(
      std::move(connection), std::move(socket), GetContext()->GetQuicClock(),
      this, DefaultQuicConfig(), GetVersions(), url_, std::move(crypto_config),
      origin, this);
  session_->Initialize();
  session_->CryptoConnect();

  return ret;
}

RawQuicError RawQuic::ConfigureSocket(DatagramClientSocket* socket,
                                      const net::IPEndPoint& dest) {
  RawQuicError ret = {RAW_QUIC_ERROR_CODE_SUCCESS, 0, 0};
  do {
    if (socket == nullptr) {
      ret.error = RAW_QUIC_ERROR_CODE_INVALID_PARAM;
      break;
    }

    socket->UseNonBlockingIO();

    ret.net_error = socket->Connect(dest);
    if (ret.net_error != net::OK) {
      ret.error = RAW_QUIC_ERROR_CODE_SOCKET_ERROR;
      break;
    }

    ret.net_error = socket->SetReceiveBufferSize(kQuicSocketReceiveBufferSize);
    if (ret.net_error != net::OK) {
      ret.error = RAW_QUIC_ERROR_CODE_SOCKET_ERROR;
      break;
    }

    ret.net_error = socket->SetDoNotFragment();
    // SetDoNotFragment is not implemented on all platforms, so ignore errors.
    if (ret.net_error != net::OK && ret.net_error != net::ERR_NOT_IMPLEMENTED) {
      ret.error = RAW_QUIC_ERROR_CODE_SOCKET_ERROR;
      break;
    }

    // Set a buffer large enough to contain the initial CWND's worth of packet
    // to work around the problem with CHLO packets being sent out with the
    // wrong encryption level, when the send buffer is full.
    ret.net_error =
        socket->SetSendBufferSize(quic::kMaxOutgoingPacketSize * 20);
    if (ret.net_error != net::OK) {
      ret.error = RAW_QUIC_ERROR_CODE_SOCKET_ERROR;
      break;
    }
  } while (0);

  return ret;
}

std::unique_ptr<quic::QuicConnection> RawQuic::CreateConnection(
    DatagramClientSocket* socket,
    const net::IPEndPoint& dest) {
  quic::QuicConnectionId connection_id =
      quic::QuicUtils::CreateRandomConnectionId(GetContext()->GetQuicRandom());

  net::QuicChromiumPacketWriter* writer =
      new net::QuicChromiumPacketWriter(socket, GetContext()->GetTaskRunner());

  auto connection = std::make_unique<quic::QuicConnection>(
      connection_id, net::ToQuicSocketAddress(dest),
      GetContext()->GetQuicConnectionHelper(),
      GetContext()->GetQuicAlarmFactory(), writer, true /* owns_writer */,
      quic::Perspective::IS_CLIENT, GetVersions());

  return connection;
}

quic::ParsedQuicVersionVector RawQuic::GetVersions() {
  quic::ParsedQuicVersionVector versions;
  versions.emplace_back(quic::PROTOCOL_TLS1_3, quic::QUIC_VERSION_99);
  return versions;
}

quic::QuicConfig RawQuic::DefaultQuicConfig() {
  quic::QuicConfig config;
  config.SetIdleNetworkTimeout(
      quic::QuicTime::Delta::FromSeconds(kMaxIdleNetworkTimeout),
      quic::QuicTime::Delta::FromSeconds(kDefaultIdleNetworkTimeout));
  config.set_max_time_before_crypto_handshake(
      quic::QuicTime::Delta::FromSeconds(kSetMaxTimeBeforeCryptoHandshake));
  config.set_max_idle_time_before_crypto_handshake(
      quic::QuicTime::Delta::FromSeconds(kSetMaxIdleTimeBeforeCryptoHandshake));
  return config;
}

void RawQuic::FlushWriteBuffer() {
  while (can_write_) {
    if (write_queue_.empty() || stream_ == nullptr) {
      break;
    }

    quic::QuicData& data = write_queue_.front();
    if (!stream_->Write(quiche::QuicheStringPiece(data.data(), data.length()))) {
      can_write_ = false;
      break;
    }

    buffered_write_data_size_ -= data.length();
    write_queue_.pop();
  }
}

void RawQuic::FillReadBuffer() {
  while (read_buffer_.size() < recv_buffer_size_) {
    if (stream_ == nullptr) {
      break;
    }

    uint32_t read_len =
        stream_->Read((char*)temp_read_buffer_.get(), kReadOnceSize);
    if (read_len == 0) {
      break;
    }

    read_ostream_.write((const char*)temp_read_buffer_.get(), read_len);
  }
}

void RawQuic::ReportError(RawQuicError* error) {
  int32_t status = status_.load();
  if (status == RAW_QUIC_STATUS_CONNECTED) {
    if (callback_.error_callback != nullptr) {
      callback_.error_callback(this, error, opaque_);
    }
  } else {
    if (connect_promise_ != nullptr) {
      connect_promise_->set_value(
          error != nullptr ? error->error : RAW_QUIC_ERROR_CODE_UNKNOWN);
      connect_promise_ = nullptr;
    } else {
      if (callback_.connect_callback != nullptr) {
        callback_.connect_callback(this, error, opaque_);
      }
    }
  }
}

void RawQuic::OnClosed(RawQuicError* error) {
  ReportError(error);
  status_.store(RAW_QUIC_STATUS_CLOSED);
}

void RawQuic::OnSessionReady() {
  status_.store(RAW_QUIC_STATUS_CONNECTED);

  std::unique_ptr<quic::QuicTransportStream::Visitor> stream_visitor =
      std::make_unique<RawQuicStreamVisitor>(this);
  stream_ = session_->OpenOutgoingBidirectionalStream();
  stream_->set_visitor(std::move(stream_visitor));

  if (connect_promise_ != nullptr) {
    connect_promise_->set_value(RAW_QUIC_ERROR_CODE_SUCCESS);
    connect_promise_ = nullptr;
  } else if (callback_.connect_callback != nullptr) {
    RawQuicError ret = {RAW_QUIC_ERROR_CODE_SUCCESS, 0, 0};
    callback_.connect_callback(this, &ret, opaque_);
  }
}

void RawQuic::OnIncomingBidirectionalStreamAvailable() {
  // TBD
}

void RawQuic::OnIncomingUnidirectionalStreamAvailable() {
  // TBD
}

void RawQuic::OnDatagramReceived(quiche::QuicheStringPiece datagram) {

}

void RawQuic::OnCanCreateNewOutgoingBidirectionalStream() {

}

void RawQuic::OnCanCreateNewOutgoingUnidirectionalStream() {

}

void RawQuic::OnConnectionClosed(quic::QuicConnectionId server_connection_id,
                                 quic::QuicErrorCode error,
                                 const std::string& error_details,
                                 quic::ConnectionCloseSource source) {
  if (error != quic::QUIC_NO_ERROR) {
    RawQuicError ret = {RAW_QUIC_ERROR_CODE_QUIC_ERROR,
                        net::ERR_QUIC_PROTOCOL_ERROR, error};
    OnClosed(&ret);
  }

  LOG(ERROR) << "Connection closed, error:" << error << ", "
             << "details: " << error_details << ".";
}

void RawQuic::OnWriteBlocked(quic::QuicBlockedWriterInterface* blocked_writer) {
  can_write_ = false;
}

void RawQuic::OnRstStreamReceived(const quic::QuicRstStreamFrame& frame) {
  if (session_ != nullptr) {
    quic::QuicConnection* connection = session_->connection();
    if (connection != nullptr) {
      connection->CloseConnection(
          quic::QUIC_NO_ERROR, "Stream reset.",
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    }
    session_ = nullptr;
  }

  RawQuicError ret = {RAW_QUIC_ERROR_CODE_STREAM_RESET, 0, 0};
  OnClosed(&ret);
}

void RawQuic::OnStopSendingReceived(const quic::QuicStopSendingFrame& frame) {
  // TBD
}

void RawQuic::OnCanRead() {
  std::unique_lock<std::mutex> lock(read_mutex_);
  FillReadBuffer();
  if (read_buffer_.size() > 0) {
    read_cond_.notify_all();
    if (callback_.can_read_callback != nullptr) {
      callback_.can_read_callback(this, read_buffer_.size(), opaque_);
    }
  }
}

void RawQuic::OnFinRead() {
  if (session_ != nullptr) {
    quic::QuicConnection* connection = session_->connection();
    if (connection != nullptr) {
      connection->CloseConnection(
          quic::QUIC_NO_ERROR, "Stream fin.",
          quic::ConnectionCloseBehavior::SEND_CONNECTION_CLOSE_PACKET);
    }
    session_ = nullptr;
  }

  RawQuicError ret = {RAW_QUIC_ERROR_CODE_STREAM_FIN, 0, 0};
  OnClosed(&ret);
}

void RawQuic::OnCanWrite() {
  can_write_ = true;
  FlushWriteBuffer();
}

}  // namespace net
