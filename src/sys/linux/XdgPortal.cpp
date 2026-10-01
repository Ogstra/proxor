#include "XdgPortal.hpp"

#include <QDBusArgument>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QPointer>
#include <QRandomGenerator>
#include <QTimer>
#include <QMetaObject>

namespace ProxorDesktop {

namespace {

const char kRequestIface[] = "org.freedesktop.portal.Request";
const char kPortalPath[] = "/org/freedesktop/portal/desktop";

QString SanitizeUnique(const QString &uniqueName) {
    QString s = uniqueName;
    if (s.startsWith(QLatin1Char(':'))) s.remove(0, 1);
    s.replace(QLatin1Char('.'), QLatin1Char('_'));
    return s;
}

// One in-flight portal request. Lives on the thread that called request(); delivers to the context queued.
class RequestWaiter : public QObject {
    Q_OBJECT
public:
    QDBusConnection bus;
    QString service;
    QString subscribedPath;
    QPointer<QObject> context;
    std::function<void(const PortalReply &)> done;
    QTimer timer;
    bool finished = false;

    RequestWaiter(const QDBusConnection &b, const QString &s) : bus(b), service(s) {
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, this, [this] {
            PortalReply r;
            r.timedOut = true;
            finish(r);
        });
    }

    bool subscribe(const QString &path) {
        unsubscribe();
        subscribedPath = path;
        return bus.connect(service, path, QString::fromLatin1(kRequestIface), QStringLiteral("Response"), this,
                           SLOT(onResponse(uint, QVariantMap)));
    }
    void unsubscribe() {
        if (subscribedPath.isEmpty()) return;
        bus.disconnect(service, subscribedPath, QString::fromLatin1(kRequestIface), QStringLiteral("Response"), this,
                       SLOT(onResponse(uint, QVariantMap)));
        subscribedPath.clear();
    }

    void finish(const PortalReply &reply) {
        if (finished) return;
        finished = true;
        timer.stop();
        unsubscribe();
        if (context) {
            auto cb = done;
            QMetaObject::invokeMethod(context.data(), [cb, reply] { cb(reply); }, Qt::QueuedConnection);
        }
        deleteLater();
    }

public slots:
    void onResponse(uint response, const QVariantMap &results) {
        PortalReply r;
        r.response = response;
        r.results = results;
        finish(r);
    }
};

} // namespace

XdgPortalClient::XdgPortalClient(QDBusConnection bus, QString service) : bus_(std::move(bus)), service_(std::move(service)) {}

QDBusConnection XdgPortalClient::bus() const { return bus_; }

QString XdgPortalClient::RequestPath(const QString &uniqueName, const QString &token) {
    return QStringLiteral("/org/freedesktop/portal/desktop/request/%1/%2").arg(SanitizeUnique(uniqueName), token);
}

QString XdgPortalClient::SessionPath(const QString &uniqueName, const QString &token) {
    return QStringLiteral("/org/freedesktop/portal/desktop/session/%1/%2").arg(SanitizeUnique(uniqueName), token);
}

QString XdgPortalClient::NewToken() {
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
    QString t = QStringLiteral("proxor_");
    for (int i = 0; i < 16; ++i) t += QLatin1Char(alphabet[QRandomGenerator::global()->bounded(36)]);
    return t;
}

QDBusMessage XdgPortalClient::call(const QString &path, const QString &iface, const QString &method,
                                   const QVariantList &args, int timeoutMs) const {
    QDBusMessage m = QDBusMessage::createMethodCall(service_, path, iface, method);
    m.setArguments(args);
    return bus_.call(m, QDBus::Block, timeoutMs);
}

uint XdgPortalClient::interfaceVersion(const QString &iface, int timeoutMs) const {
    if (!bus_.isConnected() || timeoutMs <= 0) return 0;
    const QDBusMessage reply = call(QString::fromLatin1(kPortalPath), QStringLiteral("org.freedesktop.DBus.Properties"),
                                    QStringLiteral("Get"), {iface, QStringLiteral("version")}, timeoutMs);
    if (reply.type() != QDBusMessage::ReplyMessage || reply.arguments().isEmpty()) return 0;
    QVariant v = reply.arguments().at(0);
    if (v.canConvert<QDBusVariant>()) v = v.value<QDBusVariant>().variant();
    return v.toUInt();
}

void XdgPortalClient::registerHostApp(const QString &appId, int timeoutMs) const {
    if (!bus_.isConnected() || timeoutMs <= 0) return;
    call(QString::fromLatin1(kPortalPath), QStringLiteral("org.freedesktop.host.portal.Registry"), QStringLiteral("Register"),
         {appId, QVariant::fromValue(QVariantMap())}, timeoutMs);
}

void XdgPortalClient::request(const QString &iface, const QString &method, const QVariantList &args, QVariantMap options,
                              int timeoutMs, QObject *context, std::function<void(const PortalReply &)> done) const {
    auto *w = new RequestWaiter(bus_, service_);
    w->context = context;
    w->done = std::move(done);
    if (context) QObject::connect(context, &QObject::destroyed, w, [w] { w->finished = true; w->timer.stop(); w->unsubscribe(); w->deleteLater(); });

    auto failAsync = [w](const QString &err) {
        PortalReply r;
        r.error = err;
        QMetaObject::invokeMethod(w, [w, r] { w->finish(r); }, Qt::QueuedConnection);
    };
    if (!bus_.isConnected()) {
        failAsync(QStringLiteral("No D-Bus session bus."));
        return;
    }

    const QString token = NewToken();
    options.insert(QStringLiteral("handle_token"), token);
    if (!w->subscribe(RequestPath(bus_.baseService(), token))) {
        failAsync(QStringLiteral("Could not subscribe to the portal response."));
        return;
    }

    QDBusMessage m = QDBusMessage::createMethodCall(service_, QString::fromLatin1(kPortalPath), iface, method);
    QVariantList all = args;
    all << QVariant::fromValue(options);
    m.setArguments(all);
    w->timer.start(timeoutMs);

    auto *watcher = new QDBusPendingCallWatcher(bus_.asyncCall(m, qMax(timeoutMs, 1000) + 5000), w);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, w, [w, watcher] {
        watcher->deleteLater();
        if (w->finished) return;
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError()) {
            PortalReply r;
            r.error = reply.error().message().isEmpty() ? reply.error().name() : reply.error().message();
            w->finish(r);
            return;
        }
        const QString handle = reply.value().path();
        if (!handle.isEmpty() && handle != w->subscribedPath) w->subscribe(handle);
    });
}

PortalVersions ProbePortalVersions(const XdgPortalClient &client, bool sandboxed, const QString &hostAppId, int budgetMs) {
    PortalVersions v;
    if (!client.bus().isConnected()) {
        v.detail = QStringLiteral("no session D-Bus");
        return v;
    }
    QElapsedTimer clock;
    clock.start();
    auto remaining = [&] { return int(qMax<qint64>(0, budgetMs - clock.elapsed())); };
    if (!sandboxed) client.registerHostApp(hostAppId, qMin(300, remaining()));
    v.background = client.interfaceVersion(QStringLiteral("org.freedesktop.portal.Background"), remaining());
    v.screenshot = client.interfaceVersion(QStringLiteral("org.freedesktop.portal.Screenshot"), remaining());
    v.globalShortcuts = client.interfaceVersion(QStringLiteral("org.freedesktop.portal.GlobalShortcuts"), remaining());
    if (!v.background && !v.screenshot && !v.globalShortcuts) v.detail = QStringLiteral("no desktop portal service answered");
    return v;
}

PortalResult ResultFromReply(const PortalReply &reply) {
    PortalResult r;
    if (reply.timedOut) {
        r.outcome = PortalOutcome::TimedOut;
        r.detail = QStringLiteral("The desktop did not answer in time.");
    } else if (!reply.error.isEmpty()) {
        r.outcome = PortalOutcome::Failed;
        r.detail = reply.error;
    } else if (reply.response == 0) {
        r.outcome = PortalOutcome::Granted;
    } else if (reply.response == 1) {
        r.outcome = PortalOutcome::Cancelled;
    } else {
        r.outcome = PortalOutcome::Denied;
        r.detail = QStringLiteral("The desktop refused or could not complete the request.");
    }
    return r;
}

} // namespace ProxorDesktop

#include "XdgPortal.moc"
