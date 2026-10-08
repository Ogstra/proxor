#include "fake_portal.hpp"

#include "sys/linux/XdgPortal.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QMap>
#include <QMutex>
#include <QPointer>
#include <QThread>
#include <QTimer>

namespace {
const char kService[] = "org.freedesktop.portal.Desktop";
const char kPath[] = "/org/freedesktop/portal/desktop";
const char kConn[] = "fake-portal";
} // namespace

struct FakePortal::Impl : public QDBusVirtualObject {
    mutable QMutex mutex;
    QMap<QString, uint> versions;
    QMap<QString, std::function<FakePortalAnswer(const FakePortalCall &)>> requestHandlers;
    QMap<QString, std::function<QDBusMessage(const QDBusMessage &)>> directHandlers;
    QList<FakePortalCall> recorded;
    QThread thread;
    bool registered = false;

    QString introspect(const QString &) const override { return QString(); }

    static QVariantList Convert(const QVariantList &in) {
        QVariantList out;
        for (const QVariant &v : in) {
            if (v.canConvert<QDBusArgument>()) {
                const QDBusArgument arg = v.value<QDBusArgument>();
                if (arg.currentType() == QDBusArgument::MapType) {
                    out << QVariant::fromValue(qdbus_cast<QVariantMap>(arg));
                    continue;
                }
            }
            out << v;
        }
        return out;
    }

    bool handleMessage(const QDBusMessage &msg, const QDBusConnection &conn) override {
        if (msg.type() != QDBusMessage::MethodCallMessage) return false;
        FakePortalCall c;
        c.path = msg.path();
        c.interface = msg.interface();
        c.member = msg.member();
        c.sender = msg.service();
        c.args = Convert(msg.arguments());
        std::function<FakePortalAnswer(const FakePortalCall &)> rh;
        std::function<QDBusMessage(const QDBusMessage &)> dh;
        uint version = 0;
        bool haveVersion = false;
        const QString key = c.interface + QLatin1Char('|') + c.member;
        {
            QMutexLocker l(&mutex);
            recorded << c;
            if (c.interface == QLatin1String("org.freedesktop.DBus.Properties") && c.member == QLatin1String("Get") && c.args.size() == 2) {
                haveVersion = versions.contains(c.args.at(0).toString()) && c.args.at(1).toString() == QLatin1String("version");
                if (haveVersion) version = versions.value(c.args.at(0).toString());
            } else {
                rh = requestHandlers.value(key);
                dh = directHandlers.value(key);
            }
        }
        if (c.interface == QLatin1String("org.freedesktop.DBus.Properties")) {
            if (!haveVersion) return conn.send(msg.createErrorReply(QDBusError::InvalidArgs, QStringLiteral("no such property")));
            QDBusMessage reply = msg.createReply();
            reply << QVariant::fromValue(QDBusVariant(QVariant::fromValue(version)));
            return conn.send(reply);
        }
        if (dh) return conn.send(dh(msg));
        if (rh) {
            const FakePortalAnswer a = rh(c);
            if (!a.errorName.isEmpty()) return conn.send(msg.createErrorReply(a.errorName, QStringLiteral("fake portal error")));
            QVariantMap options;
            if (!c.args.isEmpty()) options = c.args.last().toMap();
            const QString path = a.overrideRequestPath.isEmpty()
                                     ? ProxorDesktop::XdgPortalClient::RequestPath(c.sender, options.value(QStringLiteral("handle_token")).toString())
                                     : a.overrideRequestPath;
            QDBusMessage reply = msg.createReply();
            reply << QVariant::fromValue(QDBusObjectPath(path));
            const bool ok = conn.send(reply);
            if (a.emitResponse) {
                QDBusConnection c2 = conn;
                const uint response = a.response;
                const QVariantMap results = a.results;
                QTimer::singleShot(a.delayMs, this, [c2, path, response, results]() mutable {
                    QDBusMessage sig = QDBusMessage::createSignal(path, QStringLiteral("org.freedesktop.portal.Request"), QStringLiteral("Response"));
                    sig << QVariant::fromValue(response) << QVariant::fromValue(results);
                    c2.send(sig);
                });
            }
            return ok;
        }
        return conn.send(msg.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("unknown")));
    }
};

FakePortal::FakePortal() : d(new Impl) {
    d->moveToThread(&d->thread);
    d->thread.start();
}

FakePortal::~FakePortal() {
    if (d->registered) {
        QMetaObject::invokeMethod(d.get(), [] {
            auto bus = QDBusConnection(QString::fromLatin1(kConn));
            bus.unregisterObject(QString::fromLatin1(kPath));
            bus.unregisterService(QString::fromLatin1(kService));
        }, Qt::BlockingQueuedConnection);
        QDBusConnection::disconnectFromBus(QString::fromLatin1(kConn));
    }
    d->thread.quit();
    d->thread.wait();
}

bool FakePortal::start() {
    bool ok = false;
    QMetaObject::invokeMethod(d.get(), [&] {
        auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString::fromLatin1(kConn));
        ok = bus.isConnected() && bus.registerService(QString::fromLatin1(kService))
             && bus.registerVirtualObject(QString::fromLatin1(kPath), d.get(), QDBusConnection::SubPath);
    }, Qt::BlockingQueuedConnection);
    d->registered = ok;
    return ok;
}

void FakePortal::setVersion(const QString &iface, uint version) {
    QMutexLocker l(&d->mutex);
    d->versions.insert(iface, version);
}

void FakePortal::setRequestHandler(const QString &iface, const QString &member, std::function<FakePortalAnswer(const FakePortalCall &)> h) {
    QMutexLocker l(&d->mutex);
    d->requestHandlers.insert(iface + QLatin1Char('|') + member, std::move(h));
}

void FakePortal::setDirectHandler(const QString &iface, const QString &member, std::function<QDBusMessage(const QDBusMessage &)> h) {
    QMutexLocker l(&d->mutex);
    d->directHandlers.insert(iface + QLatin1Char('|') + member, std::move(h));
}

void FakePortal::emitSignal(const QString &path, const QString &iface, const QString &member, const QVariantList &args) {
    QMetaObject::invokeMethod(d.get(), [=] {
        auto bus = QDBusConnection(QString::fromLatin1(kConn));
        QDBusMessage sig = QDBusMessage::createSignal(path, iface, member);
        sig.setArguments(args);
        bus.send(sig);
    }, Qt::BlockingQueuedConnection);
}

QList<FakePortalCall> FakePortal::calls() const {
    QMutexLocker l(&d->mutex);
    return d->recorded;
}
