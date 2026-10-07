#pragma once
// In-memory fake net.connman.iwd for tests (header-only, reused by the chain test).
// Lives on its own QThread with its own named session-bus connection, so the reader's blocking
// calls really cross the bus daemon.
#include "sys/wifi/WifiBackendIwd.hpp"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QList>
#include <QMap>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QThread>

struct FakeIwdStation {
    QString state;
    QString network;  // object path; "" = no ConnectedNetwork
};
struct FakeIwdNetwork {
    QString name;
};

class FakeIwd : public QDBusVirtualObject {
public:
    QMutex mutex;
    QList<FakeIwdStation> stations;          // station i at /net/connman/iwd/0/<i+1> (also gets a Device)
    QMap<QString, FakeIwdNetwork> networks;  // object path -> network
    int extraDevicesWithoutStation = 0;      // /net/connman/iwd/1/<k> Device only (powered off)
    int delayMs = 0;
    int calls = 0;                           // GetManagedObjects calls received
    bool malformed = false;                  // reply with a plain string instead of a{oa{sa{sv}}}
    bool denied = false;                     // reply org.freedesktop.DBus.Error.AccessDenied
    bool lastAutoStart = true;               // msg.autoStartService() of the last GetManagedObjects call

    static QString stationPath(int i) { return QStringLiteral("/net/connman/iwd/0/%1").arg(i + 1); }
    static QString networkPath(int i) { return QStringLiteral("/net/connman/iwd/0/1/net%1").arg(i); }

    QString introspect(const QString &) const override { return QString(); }

    bool handleMessage(const QDBusMessage &msg, const QDBusConnection &conn) override {
        if (msg.type() != QDBusMessage::MethodCallMessage) return false;
        if (msg.member() != QLatin1String("GetManagedObjects") || msg.path() != QLatin1String("/"))
            return conn.send(msg.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("unknown")));
        int delay;
        {
            QMutexLocker l(&mutex);
            delay = delayMs;
        }
        if (delay > 0) QThread::msleep(delay);
        QMutexLocker l(&mutex);
        ++calls;
        lastAutoStart = msg.autoStartService();
        if (denied)
            return conn.send(msg.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.AccessDenied"), QStringLiteral("denied")));
        if (malformed) {
            QDBusMessage reply = msg.createReply();
            reply << QStringLiteral("not an object map");
            return conn.send(reply);
        }
        qDBusRegisterMetaType<IwdInterfaces>();
        qDBusRegisterMetaType<IwdManagedObjects>();

        QSet<QString> connectedNetworks;
        for (const FakeIwdStation &s : stations)
            if (s.state == QLatin1String("connected") && !s.network.isEmpty()) connectedNetworks.insert(s.network);

        IwdManagedObjects objects;
        for (int i = 0; i < stations.size(); ++i) {
            const FakeIwdStation &s = stations.at(i);
            IwdInterfaces ifaces;
            QVariantMap dev;
            dev.insert(QStringLiteral("Name"), QStringLiteral("wlan%1").arg(i));
            dev.insert(QStringLiteral("Powered"), true);
            dev.insert(QStringLiteral("Mode"), QStringLiteral("station"));
            ifaces.insert(QStringLiteral("net.connman.iwd.Device"), dev);
            QVariantMap st;
            st.insert(QStringLiteral("State"), s.state);
            if (!s.network.isEmpty()) st.insert(QStringLiteral("ConnectedNetwork"), QVariant::fromValue(QDBusObjectPath(s.network)));
            ifaces.insert(QStringLiteral("net.connman.iwd.Station"), st);
            objects.insert(QDBusObjectPath(stationPath(i)), ifaces);
        }
        for (auto it = networks.constBegin(); it != networks.constEnd(); ++it) {
            IwdInterfaces ifaces;
            QVariantMap net;
            net.insert(QStringLiteral("Name"), it.value().name);
            net.insert(QStringLiteral("Connected"), connectedNetworks.contains(it.key()));
            net.insert(QStringLiteral("Type"), QStringLiteral("psk"));
            ifaces.insert(QStringLiteral("net.connman.iwd.Network"), net);
            objects.insert(QDBusObjectPath(it.key()), ifaces);
        }
        for (int k = 0; k < extraDevicesWithoutStation; ++k) {
            IwdInterfaces ifaces;
            QVariantMap dev;
            dev.insert(QStringLiteral("Name"), QStringLiteral("wlanx%1").arg(k));
            dev.insert(QStringLiteral("Powered"), false);
            dev.insert(QStringLiteral("Mode"), QStringLiteral("station"));
            ifaces.insert(QStringLiteral("net.connman.iwd.Device"), dev);
            objects.insert(QDBusObjectPath(QStringLiteral("/net/connman/iwd/1/%1").arg(k + 1)), ifaces);
        }
        QDBusMessage reply = msg.createReply();
        reply << QVariant::fromValue(objects);
        return conn.send(reply);
    }
};

namespace fake_iwd_detail {
inline const char *Service() { return "net.connman.iwd"; }
}

// Registers service "net.connman.iwd" + the virtual object at "/" on a named session-bus connection.
// Callers that want "iwd absent" simply do not call StartFakeIwd.
inline bool StartFakeIwd(QThread *&thread, FakeIwd *&fake, const QString &connName) {
    thread = new QThread;
    fake = new FakeIwd;
    fake->moveToThread(thread);
    thread->start();
    bool ok = false;
    FakeIwd *f = fake;
    QMetaObject::invokeMethod(f, [&ok, f, connName] {
        auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, connName);
        ok = bus.isConnected() && bus.registerService(QString::fromLatin1(fake_iwd_detail::Service())) &&
             bus.registerVirtualObject(QStringLiteral("/"), f, QDBusConnection::SubPath);
    }, Qt::BlockingQueuedConnection);
    return ok;
}

inline void StopFakeIwd(QThread *&thread, FakeIwd *&fake, const QString &connName, bool registered) {
    if (!thread) return;
    if (registered) {
        QMetaObject::invokeMethod(fake, [connName] {
            auto bus = QDBusConnection(connName);
            bus.unregisterObject(QStringLiteral("/"));
            bus.unregisterService(QString::fromLatin1(fake_iwd_detail::Service()));
        }, Qt::BlockingQueuedConnection);
        QDBusConnection::disconnectFromBus(connName);
    }
    thread->quit();
    thread->wait();
    delete fake;
    delete thread;
    fake = nullptr;
    thread = nullptr;
}
