#pragma once

// Tun interface addresses and the Linux IPv6 policy. Qt Core only, no Q_OS_, no app globals.

#include "platform/PlatformCapabilities.hpp"

#include <QString>
#include <QStringList>

namespace ProxorPlatform {

inline constexpr char kTunIpv4Address[] = "172.19.0.1/28";
inline constexpr char kTunIpv6Address[] = "fdfe:dcba:9876::1/126";

QStringList TunAddresses(bool includeIpv6);

enum class Ipv6KernelState { Available, Disabled, Unknown };

Ipv6KernelState ReadIpv6KernelState(const QString &procRoot);

bool EffectiveTunIpv6(HostOs os, bool requested, Ipv6KernelState state);

QString TunIpv6DroppedNotice();

} // namespace ProxorPlatform
