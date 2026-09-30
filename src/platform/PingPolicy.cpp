#include "platform/PingPolicy.hpp"

#include <QCoreApplication>

namespace ProxorPlatform {

bool IsLocalIcmpFailure(const QString &error) {
    return error.startsWith(QString::fromLatin1(kLocalIcmpErrorPrefix), Qt::CaseSensitive);
}

PingOutcome ClassifyPingResult(int mode, const QString &error) {
    if (error.isEmpty()) return PingOutcome::Latency;
    if (mode == kIcmpPingMode && IsLocalIcmpFailure(error)) return PingOutcome::RetryWithTcp;
    return PingOutcome::Unavailable;
}

int EffectivePingMode(int requestedMode, bool icmpUnavailableThisSession) {
    if (requestedMode == kIcmpPingMode && icmpUnavailableThisSession) return kTcpPingMode;
    return requestedMode;
}

QString IcmpFallbackNotice(const QString &error) {
    QString cause = error;
    if (IsLocalIcmpFailure(cause)) cause.remove(0, int(qstrlen(kLocalIcmpErrorPrefix)));
    return QCoreApplication::translate("PingPolicy",
                                       "ICMP ping is not permitted on this system (%1). Proxor measures latency with TCP ping instead for the rest of this session. Settings > General > Ping type chooses the method.")
        .arg(cause);
}

} // namespace ProxorPlatform
