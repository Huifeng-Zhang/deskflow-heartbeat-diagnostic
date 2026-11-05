/*
 * Deskflow -- mouse and keyboard sharing utility
 * Modified 2026-11-05: heartbeat timing diagnostics and wire-regression coverage.
 * SPDX-FileCopyrightText: (C) 2025 Deskflow Developers
 * SPDX-FileCopyrightText: (C) 2012 - 2016 Synergy App Ltd
 * SPDX-FileCopyrightText: (C) 2006 Chris Schoeneman
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_3.h"

#include "base/IEventQueue.h"
#include "base/Log.h"
#include "deskflow/ProtocolUtil.h"

#include <atomic>
#include <chrono>
#include <cstring>

namespace {
std::atomic<uint64_t> hbDiagnosticNextConnection{1};

int64_t hbDiagnosticNowUs() noexcept
{
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch()
  ).count();
}
} // namespace

//
// ClientProxy1_3
//

ClientProxy1_3::ClientProxy1_3(const std::string &name, deskflow::IStream *stream, IEventQueue *events)
    : ClientProxy1_2(name, stream, events),
      m_events(events)
{
  m_hbDiagConnection = hbDiagnosticNextConnection.fetch_add(1, std::memory_order_relaxed);
  setHeartbeatRate(kKeepAliveRate, kKeepAliveRate * kKeepAlivesUntilDeath);
}

ClientProxy1_3::~ClientProxy1_3()
{
  // cannot do this in superclass or our override wouldn't get called
  removeHeartbeatTimer();
}

void ClientProxy1_3::mouseWheel(int32_t xDelta, int32_t yDelta)
{
  LOG_VERBOSE("send mouse wheel to \"%s\" %+d,%+d", getName().c_str(), xDelta, yDelta);
  ProtocolUtil::writef(getStream(), kMsgDMouseWheel, xDelta, yDelta);
}

bool ClientProxy1_3::parseMessage(const uint8_t *code)
{
  // process message
  if (memcmp(code, kMsgCKeepAlive, 4) == 0) {
    LOG_INFO(
        "HB_DIAG echo_received conn=%llu last_submitted_seq=%llu mono_us=%lld",
        static_cast<unsigned long long>(m_hbDiagConnection),
        static_cast<unsigned long long>(m_hbDiagSequence),
        static_cast<long long>(hbDiagnosticNowUs())
    );
    // reset alarm
    resetHeartbeatTimer();
    return true;
  } else {
    return ClientProxy1_2::parseMessage(code);
  }
}

void ClientProxy1_3::resetHeartbeatRate()
{
  setHeartbeatRate(kKeepAliveRate, kKeepAliveRate * kKeepAlivesUntilDeath);
}

void ClientProxy1_3::setHeartbeatRate(double rate, double)
{
  m_keepAliveRate = rate;
  ClientProxy1_2::setHeartbeatRate(rate, rate * kKeepAlivesUntilDeath);
}

void ClientProxy1_3::resetHeartbeatTimer()
{
  // reset the alarm but not the keep alive timer
  ClientProxy1_2::removeHeartbeatTimer();
  ClientProxy1_2::addHeartbeatTimer();
}

void ClientProxy1_3::addHeartbeatTimer()
{
  // create and install a timer to periodically send keep alives
  if (m_keepAliveRate > 0.0) {
    const auto armedUs = hbDiagnosticNowUs();
    m_keepAliveTimer = m_events->newTimer(m_keepAliveRate, nullptr);
    m_hbDiagExpectedDueUs = armedUs + static_cast<int64_t>(m_keepAliveRate * 1000000.0);
    LOG_INFO(
        "HB_DIAG timer_armed conn=%llu mono_us=%lld expected_due_us=%lld period_us=%lld",
        static_cast<unsigned long long>(m_hbDiagConnection), static_cast<long long>(armedUs),
        static_cast<long long>(m_hbDiagExpectedDueUs),
        static_cast<long long>(m_keepAliveRate * 1000000.0)
    );
    m_events->addHandler(EventTypes::Timer, m_keepAliveTimer, [this](const auto &) { keepAlive(); });
  }

  // superclass does the alarm
  ClientProxy1_2::addHeartbeatTimer();
}

void ClientProxy1_3::removeHeartbeatTimer()
{
  // remove the timer that sends keep alives periodically
  if (m_keepAliveTimer != nullptr) {
    LOG_INFO(
        "HB_DIAG timer_cancelled conn=%llu mono_us=%lld",
        static_cast<unsigned long long>(m_hbDiagConnection),
        static_cast<long long>(hbDiagnosticNowUs())
    );
    m_events->removeHandler(EventTypes::Timer, m_keepAliveTimer);
    m_events->deleteTimer(m_keepAliveTimer);
    m_keepAliveTimer = nullptr;
    m_hbDiagExpectedDueUs = 0;
  }

  // superclass does the alarm
  ClientProxy1_2::removeHeartbeatTimer();
}

void ClientProxy1_3::keepAlive()
{
  const auto callbackUs = hbDiagnosticNowUs();
  ++m_hbDiagSequence;
  // Estimated deadline: the unchanged EventQueue resets its periodic timer
  // before dispatch. This does not observe the internal expiry decision.
  LOG_INFO(
      "HB_DIAG timer_callback conn=%llu seq=%llu mono_us=%lld expected_due_us=%lld estimated_late_us=%lld",
      static_cast<unsigned long long>(m_hbDiagConnection),
      static_cast<unsigned long long>(m_hbDiagSequence), static_cast<long long>(callbackUs),
      static_cast<long long>(m_hbDiagExpectedDueUs),
      static_cast<long long>(m_hbDiagExpectedDueUs > 0 ? callbackUs - m_hbDiagExpectedDueUs : 0)
  );
  m_hbDiagExpectedDueUs = callbackUs + static_cast<int64_t>(m_keepAliveRate * 1000000.0);
  const auto submitBeginUs = hbDiagnosticNowUs();
  LOG_INFO(
      "HB_DIAG submit_begin conn=%llu seq=%llu mono_us=%lld",
      static_cast<unsigned long long>(m_hbDiagConnection),
      static_cast<unsigned long long>(m_hbDiagSequence), static_cast<long long>(submitBeginUs)
  );
  ProtocolUtil::writef(getStream(), kMsgCKeepAlive);
  const auto submitEndUs = hbDiagnosticNowUs();
  // writef queues bytes; this is deliberately NOT labelled wire_sent.
  LOG_INFO(
      "HB_DIAG submit_return conn=%llu seq=%llu mono_us=%lld elapsed_us=%lld",
      static_cast<unsigned long long>(m_hbDiagConnection),
      static_cast<unsigned long long>(m_hbDiagSequence), static_cast<long long>(submitEndUs),
      static_cast<long long>(submitEndUs - submitBeginUs)
  );
}
