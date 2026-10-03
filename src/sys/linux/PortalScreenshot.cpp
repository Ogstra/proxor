// Screenshot through org.freedesktop.portal.Screenshot (Wayland and Flatpak, where Qt cannot grab the screen).
#include "sys/linux/PortalScreenshot.hpp"

#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QStandardPaths>
#include <QUrl>
#include <QtGlobal>

namespace ProxorDesktop {

namespace {

const char kIface[] = "org.freedesktop.portal.Screenshot";

void Deliver(QObject *context, const std::function<void(const ScreenshotResult &)> &done, PortalOutcome outcome,
             const QString &detail) {
    if (!context) return;
    QMetaObject::invokeMethod(context, [done, outcome, detail] {
        ScreenshotResult r;
        r.result.outcome = outcome;
        r.result.detail = detail;
        done(r);
    }, Qt::QueuedConnection);
}

QStringList ProductionTempDirs() {
    QStringList dirs;
    const QString tmp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString run = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!tmp.isEmpty()) dirs << tmp;
    if (!run.isEmpty()) dirs << run;
    return dirs;
}

} // namespace

// GNOME note: GNOME's Screenshot portal saves the file under ~/Pictures/Screenshots and never deletes it. Such a path
// is outside the temp dirs, so `temporary` is false there and Proxor must NOT delete the file (it is the user's).
// Paths below "<dir>/doc/" are document-portal files and are never treated as temporary either.
bool IsTemporaryScreenshotPath(const QString &path, const QStringList &tempDirs) {
    if (path.isEmpty()) return false;
    const QString p = QDir::cleanPath(path);
    for (const QString &d : tempDirs) {
        if (d.isEmpty()) continue;
        const QString dir = QDir::cleanPath(d);
        if (dir == QLatin1String("/")) continue; // would make everything temporary
        const QString prefix = dir + QLatin1Char('/');
        if (!p.startsWith(prefix)) continue;
        if (p.startsWith(prefix + QStringLiteral("doc/"))) return false;
        return true;
    }
    return false;
}

void TakeScreenshotWith(const XdgPortalClient &client, uint version, const QString &parentWindow, QObject *context,
                        std::function<void(const ScreenshotResult &)> done, int timeoutMs) {
    if (!context) return;
    if (version == 0) {
        Deliver(context, done, PortalOutcome::NotAvailable,
                QStringLiteral("This desktop does not offer the screenshot portal."));
        return;
    }
    QVariantMap options;
    options.insert(QStringLiteral("interactive"), false); // portals older than v2 ignore this
    options.insert(QStringLiteral("modal"), true);
    client.request(QString::fromLatin1(kIface), QStringLiteral("Screenshot"), {parentWindow}, options, timeoutMs, context,
                   [done](const PortalReply &reply) {
        ScreenshotResult r;
        r.result = ResultFromReply(reply);
        if (reply.error.isEmpty() && !reply.timedOut) {
            if (reply.response == 0) {
                const QUrl url(reply.results.value(QStringLiteral("uri")).toString());
                const QString local = url.isLocalFile() ? url.toLocalFile() : QString();
                if (!local.isEmpty() && QFileInfo(local).isFile()) {
                    r.result.outcome = PortalOutcome::Granted;
                    r.result.detail.clear();
                    r.imagePath = local;
                    r.temporary = IsTemporaryScreenshotPath(local, ProductionTempDirs());
                } else {
                    r.result.outcome = PortalOutcome::Failed;
                    r.result.detail = QStringLiteral("The desktop returned no screenshot.");
                }
            } else if (reply.response == 1) {
                r.result.outcome = PortalOutcome::Cancelled;
                r.result.detail = QStringLiteral("The screenshot was cancelled.");
            }
        }
        done(r);
    });
}

void TakeScreenshot(const QString &parentWindow, QObject *context, std::function<void(const ScreenshotResult &)> done) {
    TakeScreenshotWith(XdgPortalClient(QDBusConnection::sessionBus()), Portals().screenshot, parentWindow, context,
                       std::move(done));
}

} // namespace ProxorDesktop
