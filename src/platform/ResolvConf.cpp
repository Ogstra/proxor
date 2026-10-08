#include "ResolvConf.hpp"

#include <QCoreApplication>
#include <QRegularExpression>
#include <QStringList>

namespace ProxorPlatform {

    ResolvConfKind ClassifyResolvConf(const QString &content) {
        const auto lines = content.split('\n');

        // Header scan, same rule as sing-box isSystemdResolvedManaged.
        for (const auto &raw: lines) {
            const auto line = raw.trimmed();
            if (line.isEmpty() || !line.startsWith('#')) break;
            if (line.contains("systemd-resolved")) return ResolvConfKind::ManagedBySystemdResolved;
        }

        for (const auto &raw: lines) {
            const auto line = raw.trimmed();
            if (line.isEmpty() || line.startsWith('#')) continue;
            const auto tokens = line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (tokens.size() >= 2 && tokens[0] == "nameserver" && (tokens[1] == "127.0.0.53" || tokens[1] == "127.0.0.54")) {
                return ResolvConfKind::UnmanagedResolvedStub;
            }
        }
        return ResolvConfKind::Other;
    }

    QString DirectDnsResolvedNotice(ResolvConfKind kind) {
        switch (kind) {
            case ResolvConfKind::ManagedBySystemdResolved:
                return QCoreApplication::translate("ResolvConf",
                                                   "[DNS] systemd-resolved manages /etc/resolv.conf: Direct DNS \"local\" uses the DNS servers systemd-resolved reports for your network.");
            case ResolvConfKind::UnmanagedResolvedStub:
                return QCoreApplication::translate("ResolvConf",
                                                   "[Warning] /etc/resolv.conf sends DNS to the systemd-resolved stub (127.0.0.53) but is not the file systemd-resolved manages, so with Tun on Direct DNS \"local\" may fail. Link /etc/resolv.conf to /run/systemd/resolve/stub-resolv.conf, or set Direct DNS to a server address in the DNS settings.");
            case ResolvConfKind::Other:
                break;
        }
        return {};
    }

    bool DirectDnsNoticeIsWarning(ResolvConfKind kind) {
        return kind == ResolvConfKind::UnmanagedResolvedStub;
    }

} // namespace ProxorPlatform
