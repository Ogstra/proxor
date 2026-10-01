// Stub from 52-04; replaced by 52-06.
#include "sys/DesktopPortal.hpp"

#include <QMetaObject>
#include <QPointer>

namespace ProxorDesktop {

void RequestAutostart(bool, const QStringList &, const QString &, QObject *context, std::function<void(const AutostartResult &)> done) {
    if (!context) return;
    QMetaObject::invokeMethod(context, [done] {
        AutostartResult r;
        r.result.outcome = PortalOutcome::NotAvailable;
        r.result.detail = QStringLiteral("Not implemented yet.");
        done(r);
    }, Qt::QueuedConnection);
}

} // namespace ProxorDesktop
