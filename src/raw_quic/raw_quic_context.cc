// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "net/quic/raw_quic/raw_quic_context.h"

#include "net/quic/platform/impl/quic_chromium_clock.h"
#include "net/quic/quic_chromium_alarm_factory.h"
#include "net/quic/quic_chromium_connection_helper.h"
#include "net/quic/quic_chromium_packet_reader.h"
#include "net/quic/quic_chromium_packet_writer.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_default_proof_providers.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_system_event_loop.h"
#include "net/third_party/quiche/src/quic/tools/fake_proof_verifier.h"

namespace net {

RawQuicContext::RawQuicContext() : event_loop_("RawQuic") {
#if defined(OS_WIN)
  WSADATA wsaData;
  WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

  if (thread_ == nullptr) {
    thread_ = std::make_unique<base::Thread>("RawQuic");
    base::Thread::Options thread_options(base::MessagePumpType::IO, 0);
    thread_->StartWithOptions(thread_options);
  }

  if (task_runner_ == nullptr && thread_ != nullptr) {
    task_runner_ = thread_->task_runner();
  }
}

RawQuicContext::~RawQuicContext() {
  if (task_runner_ != nullptr) {
    task_runner_.reset();
  }

  if (thread_ != nullptr) {
    thread_->Stop();
    thread_.reset();
  }
}

RawQuicContext* RawQuicContext::GetInstance() {
  static base::NoDestructor<RawQuicContext> instance;
  return instance.get();
}

void RawQuicContext::Post(base::OnceClosure task) {
  if (task_runner_ != nullptr) {
    task_runner_->PostTask(FROM_HERE, std::move(task));
  }
}

base::SingleThreadTaskRunner* RawQuicContext::GetTaskRunner() {
  return task_runner_.get();
}

quic::QuicAlarmFactory* RawQuicContext::GetQuicAlarmFactory() {
  if (alarm_factory_ == nullptr) {
    alarm_factory_ = std::make_unique<net::QuicChromiumAlarmFactory>(
        task_runner_.get(), GetQuicClock());
  }
  return alarm_factory_.get();
}

quic::QuicClock* RawQuicContext::GetQuicClock() {
  return quic::QuicChromiumClock::GetInstance();
}

quic::QuicRandom* RawQuicContext::GetQuicRandom() {
  return quic::QuicRandom::GetInstance();
}

quic::QuicConnectionHelperInterface* RawQuicContext::GetQuicConnectionHelper() {
  if (helper_ == nullptr) {
    helper_ = std::make_unique<net::QuicChromiumConnectionHelper>(
        GetQuicClock(), GetQuicRandom());
  }
  return helper_.get();
}

net::NetLogWithSource* RawQuicContext::GetNetLogWithSource() {
  return &net_log_;
}

std::unique_ptr<quic::ProofVerifier> RawQuicContext::CreateProofVerifier(
    const std::string& host,
    bool verify) {
  if (!verify) {
    return std::make_unique<quic::FakeProofVerifier>();
  } else {
    return quic::CreateDefaultProofVerifier(host);
  }
}

}  // namespace net
