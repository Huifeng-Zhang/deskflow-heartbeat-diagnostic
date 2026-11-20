/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include <cstdint>
#include <string_view>

namespace deskflow {
// Digests are raw SHA-256 values from successful client certificate verification.
inline constexpr bool canResumeSession(std::string_view oldPeer, std::string_view newPeer,
                                       int64_t pendingSinceUs, int64_t nowUs, double heartbeatSeconds)
{
  return oldPeer.size() == 32 && oldPeer == newPeer && pendingSinceUs > 0 && heartbeatSeconds > 0 &&
         nowUs >= pendingSinceUs && nowUs - pendingSinceUs >= heartbeatSeconds * 1000000.0;
}
} // namespace deskflow
