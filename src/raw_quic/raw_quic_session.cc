// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"
#include "net/quic/raw_quic/raw_quic_session.h"

namespace net {

RawQuicSession::RawQuicSession(
    std::unique_ptr<quic::QuicConnection> connection,
    std::unique_ptr<net::DatagramClientSocket> socket,
    quic::QuicClock* clock,
    QuicSession::Visitor* owner,
    const quic::QuicConfig& config,
    const quic::ParsedQuicVersionVector& supported_versions,
    const GURL& url,
    std::unique_ptr<quic::QuicCryptoClientConfig> crypto_config,
    url::Origin origin,
    QuicTransportClientSession::ClientVisitor* visitor)
    : QuicTransportClientSession(connection.get(),
                                 owner,
                                 config,
                                 supported_versions,
                                 url,
                                 crypto_config.get(),
                                 origin,
                                 visitor),
      socket_(std::move(socket)),
      connection_(std::move(connection)),
      crypto_config_ (std::move(crypto_config)) {
  CreatePacketReader(socket_.get(), clock);
}

RawQuicSession::~RawQuicSession() {}

void RawQuicSession::OnReadError(int result,
                                 const DatagramClientSocket* socket) {
  quic::QuicConnection* connection = QuicSession::connection();
  if (connection != nullptr) {
    connection->CloseConnection(quic::QUIC_PACKET_READ_ERROR,
                                ErrorToString(result),
                                quic::ConnectionCloseBehavior::SILENT_CLOSE);
  }
}

bool RawQuicSession::OnPacket(const quic::QuicReceivedPacket& packet,
                              const quic::QuicSocketAddress& local_address,
                              const quic::QuicSocketAddress& peer_address) {
  quic::QuicConnection* connection = QuicSession::connection();
  if (connection != nullptr) {
    connection->ProcessUdpPacket(local_address, peer_address, packet);
    return connection->connected();
  }
  return false;
}

void RawQuicSession::CreatePacketReader(net::DatagramClientSocket* socket,
                                        quic::QuicClock* clock) {
  packet_reader_.reset(new net::QuicChromiumPacketReader(
      socket, clock, this, net::kQuicYieldAfterPacketsRead,
      quic::QuicTime::Delta::FromMilliseconds(
          net::kQuicYieldAfterDurationMilliseconds),
      net::NetLogWithSource()));
  packet_reader_->StartReading();
}

}  // namespace net
