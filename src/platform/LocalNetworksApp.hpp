#pragma once

#include "platform/LocalNetworks.hpp"

namespace ProxorPlatform {

// IPv4 addresses of the interfaces that are up, running and not loopback.
QList<Ipv4Net> CurrentIpv4Interfaces();

// Attached private networks right now, without the tunnel's own address range.
QStringList CurrentPrivateNetworks();

} // namespace ProxorPlatform
