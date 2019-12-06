// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_RAW_QUIC_RAW_QUIC_CONTEXT_H_
#define NET_QUIC_RAW_QUIC_RAW_QUIC_CONTEXT_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "net/log/net_log_with_source.h"
#include "net/third_party/quiche/src/quic/core/crypto/proof_verifier.h"
#include "net/third_party/quiche/src/quic/core/crypto/quic_random.h"
#include "net/third_party/quiche/src/quic/core/quic_alarm_factory.h"
#include "net/third_party/quiche/src/quic/core/quic_connection.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_clock.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_system_event_loop.h"

namespace net {

class RawQuicContext {
 public:
  RawQuicContext();
  virtual ~RawQuicContext();
  static RawQuicContext* GetInstance();

 public:
  void Post(base::OnceClosure task);

  base::SingleThreadTaskRunner* GetTaskRunner();

  quic::QuicAlarmFactory* GetQuicAlarmFactory();

  quic::QuicClock* GetQuicClock();

  quic::QuicRandom* GetQuicRandom();

  quic::QuicConnectionHelperInterface* GetQuicConnectionHelper();

  net::NetLogWithSource* GetNetLogWithSource();

  std::unique_ptr<quic::ProofVerifier> CreateProofVerifier(
      const std::string& host,
      bool verify = true);

 protected:
  std::unique_ptr<base::Thread> thread_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<quic::QuicAlarmFactory> alarm_factory_;
  std::unique_ptr<quic::QuicConnectionHelperInterface> helper_;
  net::NetLogWithSource net_log_;
  QuicSystemEventLoop event_loop_;
};

}  // namespace net

#endif  // NET_QUIC_RAW_QUIC_RAW_QUIC_CONTEXT_H_
