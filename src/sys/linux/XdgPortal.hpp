#pragma once
// freedesktop portal client over QtDBus. QtCore + QtDBus only (no widgets, no ProxorGui) so tests compile it anywhere.
#include "sys/DesktopPortal.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

namespace ProxorDesktop {

struct PortalReply {
    uint response = 2;
    QVariantMap results;
    QString error;
    bool timedOut = false;
};

class XdgPortalClient {
public:
    explicit XdgPortalClient(QDBusConnection bus, QString service = QStringLiteral("org.freedesktop.portal.Desktop"));
    QDBusConnection bus() const;
    uint interfaceVersion(const QString &iface, int timeoutMs) const; // Properties.Get(iface, "version"), 0 on any error
    void registerHostApp(const QString &appId, int timeoutMs) const;  // host Registry.Register(appId, {}), errors ignored
    // Adds options["handle_token"], subscribes to Request.Response on the predicted path BEFORE the call, re-subscribes
    // if the returned handle differs, and calls done once (queued on context) with the Response or error/timedOut.
    void request(const QString &iface, const QString &method, const QVariantList &args, QVariantMap options,
                 int timeoutMs, QObject *context, std::function<void(const PortalReply &)> done) const;
    // Plain method call (no Request object), e.g. Session.Close.
    QDBusMessage call(const QString &path, const QString &iface, const QString &method, const QVariantList &args, int timeoutMs) const;
    static QString RequestPath(const QString &uniqueName, const QString &token);
    static QString SessionPath(const QString &uniqueName, const QString &token);
    static QString NewToken(); // "proxor_" + 16 random [a-z0-9]

private:
    QDBusConnection bus_;
    QString service_;
};

PortalVersions ProbePortalVersions(const XdgPortalClient &client, bool sandboxed, const QString &hostAppId, int budgetMs);
PortalResult ResultFromReply(const PortalReply &reply);

} // namespace ProxorDesktop
