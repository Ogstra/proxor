#pragma once

#include <QString>

namespace ProxorPlatform {

    enum class ResolvConfKind { Other, ManagedBySystemdResolved, UnmanagedResolvedStub };

    // Mirrors sing-box isSystemdResolvedManaged: scan lines in order, trimmed; an empty line or a line not
    // starting with '#' ends the header (=> not managed); a header line containing "systemd-resolved" =>
    // ManagedBySystemdResolved. Otherwise UnmanagedResolvedStub if any non-comment line is
    // `nameserver 127.0.0.53` or `nameserver 127.0.0.54`, else Other.
    ResolvConfKind ClassifyResolvConf(const QString &content);

    // Empty for Other.
    QString DirectDnsResolvedNotice(ResolvConfKind kind);

    // True only for UnmanagedResolvedStub.
    bool DirectDnsNoticeIsWarning(ResolvConfKind kind);

} // namespace ProxorPlatform
