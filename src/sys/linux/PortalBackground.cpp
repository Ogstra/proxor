#include "sys/linux/PortalBackground.hpp"

#include <QDBusConnection>
#include <QMetaObject>
#include <QPointer>

namespace ProxorDesktop {

void RequestAutostartWith(const XdgPortalClient &client, uint version, bool enable, const QStringList &commandline,
                          const QString &reason, QObject *context, std::function<void(const AutostartResult &)> done,
                          int timeoutMs) {
    if (!context) return;
    if (version == 0) {
        QPointer<QObject> ctx(context);
        QMetaObject::invokeMethod(context, [done, ctx] {
            if (!ctx) return;
            AutostartResult r;
            r.result.outcome = PortalOutcome::NotAvailable;
            r.result.detail = QStringLiteral("The desktop does not offer the Background portal.");
            done(r);
        }, Qt::QueuedConnection);
        return;
    }
    QVariantMap options;
    options.insert(QStringLiteral("reason"), reason);
    options.insert(QStringLiteral("autostart"), enable);
    options.insert(QStringLiteral("commandline"), commandline);
    options.insert(QStringLiteral("dbus-activatable"), false);
    client.request(QStringLiteral("org.freedesktop.portal.Background"), QStringLiteral("RequestBackground"),
                   {QString()}, options, timeoutMs, context, [done, enable](const PortalReply &reply) {
                       AutostartResult r;
                       r.result = ResultFromReply(reply);
                       if (r.result.outcome == PortalOutcome::Cancelled && r.result.detail.isEmpty())
                           r.result.detail = QStringLiteral("The request was cancelled.");
                       if (r.result.outcome == PortalOutcome::Granted) {
                           r.autostart = reply.results.value(QStringLiteral("autostart")).toBool();
                           if (r.autostart != enable) {
                               r.result.outcome = PortalOutcome::Denied;
                               r.result.detail = QStringLiteral("The desktop did not allow Proxor to start when you log in.");
                           }
                       }
                       done(r);
                   });
}

void RequestAutostart(bool enable, const QStringList &commandline, const QString &reason, QObject *context,
                      std::function<void(const AutostartResult &)> done) {
    RequestAutostartWith(XdgPortalClient(QDBusConnection::sessionBus()), Portals().background, enable, commandline,
                         reason, context, std::move(done));
}

} // namespace ProxorDesktop
