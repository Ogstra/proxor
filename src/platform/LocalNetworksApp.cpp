#include "platform/LocalNetworksApp.hpp"

#include <QAbstractSocket>
#include <QNetworkAddressEntry>
#include <QNetworkInterface>

namespace ProxorPlatform {

QList<Ipv4Net> CurrentIpv4Interfaces() {
    QList<Ipv4Net> result;
    for (const auto &iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) ||
            (flags & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        for (const auto &entry : iface.addressEntries()) {
            const auto ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv4Protocol) continue;
            result.append({ip.toIPv4Address(), entry.prefixLength()});
        }
    }
    return result;
}

QStringList CurrentPrivateNetworks() {
    // 172.19.0.0/28 is the tunnel's own address range (ConfigBuilder BuildTunAddressArray).
    return PrivateAttachedNetworks(CurrentIpv4Interfaces(), {QStringLiteral("172.19.0.0/28")});
}

} // namespace ProxorPlatform
