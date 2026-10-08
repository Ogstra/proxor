#pragma once

// Directly attached private IPv4 networks (Qt Core only). Proxor used to keep every RFC 1918
// range out of the tunnel and the system proxy, which also kept remote private networks (for
// example 10.10.10.0/24 behind the proxy server) from ever reaching the proxy. Only the networks
// this machine is attached to are kept local now.

#include <QList>
#include <QString>
#include <QStringList>

namespace ProxorPlatform {

struct Ipv4Net {
    quint32 address = 0; // host byte order
    int prefix = 0;      // 0..32
};

// "a.b.c.d/p" network of an RFC 1918 address (10/8, 172.16/12, 192.168/16) with 1 <= prefix <= 32;
// empty for anything else (public, loopback, link-local, CGNAT, invalid prefix).
QString PrivateNetworkCidr(const Ipv4Net &net);

// Deduplicated, sorted CIDRs of the attached private networks. Networks inside `ignored`
// (CIDR strings such as the tunnel's own address range) are dropped.
QStringList PrivateAttachedNetworks(const QList<Ipv4Net> &interfaces, const QStringList &ignored = {});

// `base` without the blanket RFC 1918 ranges (10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16), followed by
// `attached`. Every other entry of `base` keeps its place.
QStringList ReplaceBlanketPrivateRanges(const QStringList &base, const QStringList &attached);

} // namespace ProxorPlatform
