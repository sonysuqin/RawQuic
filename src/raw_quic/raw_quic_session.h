// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_RAW_QUIC_RAW_QUIC_SESSION_H_
#define NET_QUIC_RAW_QUIC_RAW_QUIC_SESSION_H_


#include "net/quic/quic_chromium_packet_reader.h"
#include "net/socket/datagram_client_socket.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_crypto_client_config.h"
#include "net/third_party/quiche/src/quic/quic_transport/quic_transport_client_session.h"

namespace net {

class RawQuicSession : public quic::QuicTransportClientSession,
                       public net::QuicChromiumPacketReader::Visitor {
 public:
  class QUIC_EXPORT_PRIVATE Visitor {
   public:
    virtual ~Visitor() {}
    virtual void OnConnectionOpened() = 0;
  };

  RawQuicSession(std::unique_ptr<quic::QuicConnection> connection,
                 std::unique_ptr<net::DatagramClientSocket> socket,
                 quic::QuicClock* clock,
                 QuicSession::Visitor* owner,
                 RawQuicSession::Visitor* raw_quic_visitor,
                 const quic::QuicConfig& config,
                 const quic::ParsedQuicVersionVector& supported_versions,
                 const quic::QuicServerId& server_id,
                 std::unique_ptr<quic::QuicCryptoClientConfig> crypto_config,
                 url::Origin origin,
                 QuicTransportClientSession::ClientVisitor* visitor);
  ~RawQuicSession() override;

  // QuicSession
  void OnCryptoHandshakeEvent(CryptoHandshakeEvent event) override;

    // net::QuicChromiumPacketReader::Visitor
  void OnReadError(int result, const DatagramClientSocket* socket) override;

  bool OnPacket(const quic::QuicReceivedPacket& packet,
                const quic::QuicSocketAddress& local_address,
                const quic::QuicSocketAddress& peer_address) override;

 protected:
  void CreatePacketReader(net::DatagramClientSocket* socket,
                          quic::QuicClock* clock);

 protected:
  RawQuicSession::Visitor* raw_quic_visitor_;
  std::unique_ptr<net::DatagramClientSocket> socket_;
  std::unique_ptr<quic::QuicConnection> connection_;
  std::unique_ptr<quic::QuicCryptoClientConfig> crypto_config_;
  std::unique_ptr<net::QuicChromiumPacketReader> packet_reader_;
};

}  // namespace net

#endif  // NET_QUIC_RAW_QUIC_RAW_QUIC_SESSION_H_
