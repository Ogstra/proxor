#include "platform/TunAddress.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace ProxorPlatform {

QStringList TunAddresses(bool includeIpv6) {
    QStringList addresses{QString::fromLatin1(kTunIpv4Address)};
    if (includeIpv6) addresses << QString::fromLatin1(kTunIpv6Address);
    return addresses;
}

static bool FlagIsOne(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    return QString::fromLatin1(f.read(16)).trimmed() == QLatin1String("1");
}

Ipv6KernelState ReadIpv6KernelState(const QString &procRoot) {
    const QDir root(procRoot);
    if (!root.exists()) return Ipv6KernelState::Unknown;
    if (!QFileInfo::exists(root.filePath(QStringLiteral("proc/net/if_inet6")))) return Ipv6KernelState::Disabled;
    if (FlagIsOne(root.filePath(QStringLiteral("proc/sys/net/ipv6/conf/all/disable_ipv6"))) ||
        FlagIsOne(root.filePath(QStringLiteral("proc/sys/net/ipv6/conf/default/disable_ipv6"))))
        return Ipv6KernelState::Disabled;
    return Ipv6KernelState::Available;
}

bool EffectiveTunIpv6(HostOs os, bool requested, Ipv6KernelState state) {
    if (os != HostOs::Linux) return requested;
    return requested && state != Ipv6KernelState::Disabled;
}

QString TunIpv6DroppedNotice() {
    return QCoreApplication::translate(
        "TunAddress",
        "[Tun] The kernel does not allow IPv6 on new network interfaces (IPv6 is off, or "
        "net.ipv6.conf.all/default.disable_ipv6 = 1), so the Tun interface gets no IPv6 address and Tun "
        "captures IPv4 only. If another network interface still has IPv6, that IPv6 traffic bypasses Tun.");
}

} // namespace ProxorPlatform
