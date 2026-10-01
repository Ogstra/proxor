// Stub from 52-04; replaced by 52-07.
#include "sys/DesktopPortal.hpp"

#include <QMetaObject>
#include <QPointer>

namespace ProxorDesktop {

void TakeScreenshot(const QString &, QObject *context, std::function<void(const ScreenshotResult &)> done) {
    if (!context) return;
    QMetaObject::invokeMethod(context, [done] {
        ScreenshotResult r;
        r.result.outcome = PortalOutcome::NotAvailable;
        r.result.detail = QStringLiteral("Not implemented yet.");
        done(r);
    }, Qt::QueuedConnection);
}

} // namespace ProxorDesktop
