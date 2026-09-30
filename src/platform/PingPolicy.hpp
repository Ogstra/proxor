#pragma once

// Pure ICMP-to-TCP ping fallback decisions (Qt Core only).

#include <QString>

namespace ProxorPlatform {

inline constexpr int kTcpPingMode = 0;  // libcore::TcpPing
inline constexpr int kIcmpPingMode = 3; // libcore::IcmpPing

// Must equal go/proxorlib/speedtest/icmp.go LocalIcmpErrorPrefix;
// test/package_mode/wiring.d/ping.sh enforces it.
inline constexpr const char *kLocalIcmpErrorPrefix = "icmp-unavailable: ";

enum class PingOutcome { Latency, Unavailable, RetryWithTcp };

// True when the core says this machine cannot send ICMP (exact prefix).
bool IsLocalIcmpFailure(const QString &error);

PingOutcome ClassifyPingResult(int mode, const QString &error);

int EffectivePingMode(int requestedMode, bool icmpUnavailableThisSession);

QString IcmpFallbackNotice(const QString &error);

} // namespace ProxorPlatform
