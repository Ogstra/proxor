// Windows and macOS: desktop portals exist only on Linux, so every portal is reported absent.
#include "sys/DesktopPortal.hpp"

#include <QMetaObject>
#include <QPointer>

namespace ProxorDesktop {

void StartPortalProbe() {}

const PortalVersions &Portals() {
    static const PortalVersions none = [] {
        PortalVersions v;
        v.detail = QStringLiteral("desktop portals exist only on Linux");
        return v;
    }();
    return none;
}

void RequestAutostart(bool, const QStringList &, const QString &, QObject *context, std::function<void(const AutostartResult &)> done) {
    if (!context) return;
    QPointer<QObject> guard(context);
    QMetaObject::invokeMethod(context, [done] {
        AutostartResult r;
        r.result.outcome = PortalOutcome::NotAvailable;
        r.result.detail = QStringLiteral("Desktop portals exist only on Linux.");
        done(r);
    }, Qt::QueuedConnection);
}

void TakeScreenshot(const QString &, QObject *context, std::function<void(const ScreenshotResult &)> done) {
    if (!context) return;
    QMetaObject::invokeMethod(context, [done] {
        ScreenshotResult r;
        r.result.outcome = PortalOutcome::NotAvailable;
        r.result.detail = QStringLiteral("Desktop portals exist only on Linux.");
        done(r);
    }, Qt::QueuedConnection);
}

std::unique_ptr<GlobalShortcutSession> CreateGlobalShortcutSession(QObject *) { return nullptr; }

} // namespace ProxorDesktop
