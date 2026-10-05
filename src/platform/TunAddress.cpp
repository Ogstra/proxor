#include "platform/TunAddress.hpp"

namespace ProxorPlatform {

// RED stub: wrong on purpose.
QStringList TunAddresses(bool) { return {}; }
Ipv6KernelState ReadIpv6KernelState(const QString &) { return Ipv6KernelState::Unknown; }
bool EffectiveTunIpv6(HostOs, bool, Ipv6KernelState) { return false; }
QString TunIpv6DroppedNotice() { return {}; }

} // namespace ProxorPlatform
