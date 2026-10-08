#include "platform/LocalNetworks.hpp"

#include <algorithm>

namespace ProxorPlatform {

namespace {

quint32 Mask(int prefix) {
    return prefix <= 0 ? 0u : (prefix >= 32 ? 0xFFFFFFFFu : (0xFFFFFFFFu << (32 - prefix)));
}

bool IsRfc1918(quint32 a) {
    return (a >> 24) == 10 || (a >> 20) == 0xAC1 /* 172.16/12 */ || (a >> 16) == 0xC0A8 /* 192.168/16 */;
}

QString Dotted(quint32 a) {
    return QStringLiteral("%1.%2.%3.%4").arg(a >> 24).arg((a >> 16) & 255).arg((a >> 8) & 255).arg(a & 255);
}

// Network start and prefix of "a.b.c.d/p"; false when it is not a valid IPv4 CIDR.
bool ParseCidr(const QString &cidr, quint32 *start, int *prefix) {
    const auto parts = cidr.split(QLatin1Char('/'));
    if (parts.size() != 2) return false;
    bool okPrefix = false;
    const int p = parts[1].toInt(&okPrefix);
    const auto octets = parts[0].split(QLatin1Char('.'));
    if (!okPrefix || p < 0 || p > 32 || octets.size() != 4) return false;
    quint32 value = 0;
    for (const auto &o : octets) {
        bool ok = false;
        const int v = o.toInt(&ok);
        if (!ok || v < 0 || v > 255) return false;
        value = (value << 8) | static_cast<quint32>(v);
    }
    *start = value & Mask(p);
    *prefix = p;
    return true;
}

} // namespace

QString PrivateNetworkCidr(const Ipv4Net &net) {
    if (net.prefix < 1 || net.prefix > 32 || !IsRfc1918(net.address)) return QString();
    return Dotted(net.address & Mask(net.prefix)) + QLatin1Char('/') + QString::number(net.prefix);
}

QStringList PrivateAttachedNetworks(const QList<Ipv4Net> &interfaces, const QStringList &ignored) {
    QStringList result;
    for (const auto &net : interfaces) {
        const QString cidr = PrivateNetworkCidr(net);
        if (cidr.isEmpty()) continue;
        quint32 start = 0;
        int prefix = 0;
        ParseCidr(cidr, &start, &prefix);
        bool skip = false;
        for (const auto &ig : ignored) {
            quint32 igStart = 0;
            int igPrefix = 0;
            // Dropped when this network lies inside an ignored one (same or shorter ignored prefix).
            if (ParseCidr(ig, &igStart, &igPrefix) && igPrefix <= prefix && (start & Mask(igPrefix)) == igStart) skip = true;
        }
        if (!skip && !result.contains(cidr)) result << cidr;
    }
    std::sort(result.begin(), result.end());
    return result;
}

QStringList ReplaceBlanketPrivateRanges(const QStringList &base, const QStringList &attached) {
    static const QStringList blanket{QStringLiteral("10.0.0.0/8"), QStringLiteral("172.16.0.0/12"),
                                     QStringLiteral("192.168.0.0/16")};
    QStringList result;
    for (const auto &entry : base) {
        if (!blanket.contains(entry)) result << entry;
    }
    for (const auto &cidr : attached) {
        if (!result.contains(cidr)) result << cidr;
    }
    return result;
}

} // namespace ProxorPlatform
