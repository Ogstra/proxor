#include "sys/wifi/WifiBackendNetworkManager.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QFile>
#include <QMap>
#include <QMutex>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

using namespace ProxorWifi;

namespace {

const char kNmService[] = "org.freedesktop.NetworkManager";
const char kNmPath[] = "/org/freedesktop/NetworkManager";

struct FakeDevice {
    quint32 type = 2;
    quint32 state = 100;
    QString activeAp = "/";
    QByteArray ssid;
};

// In-memory NetworkManager. Lives on its own thread so the reader's blocking calls are answered.
class FakeNetworkManager : public QDBusVirtualObject {
public:
    QMutex mutex;
    QList<FakeDevice> devices;  // device i is /org/freedesktop/NetworkManager/Devices/i, its AP .../AccessPoint/i
    int delayMs = 0;

    QString introspect(const QString &) const override { return QString(); }

    bool handleMessage(const QDBusMessage &msg, const QDBusConnection &conn) override {
        if (msg.type() != QDBusMessage::MethodCallMessage) return false;
        int delay;
        {
            QMutexLocker l(&mutex);
            delay = delayMs;
        }
        if (delay > 0) QThread::msleep(delay);
        QMutexLocker l(&mutex);
        if (msg.member() == "GetDevices") {
            QList<QDBusObjectPath> paths;
            for (int i = 0; i < devices.size(); ++i) paths << QDBusObjectPath(devicePath(i));
            QDBusMessage reply = msg.createReply();
            reply << QVariant::fromValue(paths);
            return conn.send(reply);
        }
        if (msg.member() == "Get" && msg.arguments().size() == 2) {
            const QString iface = msg.arguments().at(0).toString();
            const QString prop = msg.arguments().at(1).toString();
            QVariant value;
            const int dev = indexOf(msg.path(), "/Devices/");
            const int ap = indexOf(msg.path(), "/AccessPoint/");
            if (dev >= 0 && dev < devices.size()) {
                const FakeDevice &d = devices.at(dev);
                if (prop == "DeviceType") value = d.type;
                else if (prop == "State") value = d.state;
                else if (prop == "ActiveAccessPoint")
                    value = QVariant::fromValue(QDBusObjectPath(d.activeAp));
            } else if (ap >= 0 && ap < devices.size() && prop == "Ssid") {
                value = devices.at(ap).ssid;
            }
            Q_UNUSED(iface);
            if (!value.isValid()) return conn.send(msg.createErrorReply(QDBusError::InvalidArgs, "no such property"));
            QDBusMessage reply = msg.createReply();
            reply << QVariant::fromValue(QDBusVariant(value));
            return conn.send(reply);
        }
        return conn.send(msg.createErrorReply(QDBusError::UnknownMethod, "unknown"));
    }

    static QString devicePath(int i) { return QStringLiteral("/org/freedesktop/NetworkManager/Devices/%1").arg(i); }
    static QString apPath(int i) { return QStringLiteral("/org/freedesktop/NetworkManager/AccessPoint/%1").arg(i); }

private:
    static int indexOf(const QString &path, const char *marker) {
        const int at = path.indexOf(QLatin1String(marker));
        if (at < 0) return -1;
        return path.mid(at + int(strlen(marker))).toInt();
    }
};

bool IsWindowsHost() { return QSysInfo::kernelType() == QLatin1String("winnt"); }

} // namespace

class WifiNmTest : public QObject {
    Q_OBJECT
    QThread *thread_ = nullptr;
    FakeNetworkManager *fake_ = nullptr;
    bool serviceRegistered_ = false;

    void startFake(bool registerService) {
        thread_ = new QThread;
        fake_ = new FakeNetworkManager;
        fake_->moveToThread(thread_);
        thread_->start();
        if (!registerService) return;
        bool ok = false;
        QMetaObject::invokeMethod(fake_, [&] {
            // Own connection: the reader's blocking calls must cross the daemon, not shortcut in-process.
            auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-nm"));
            ok = bus.isConnected() && bus.registerService(kNmService) && bus.registerVirtualObject(kNmPath, fake_, QDBusConnection::SubPath);
        }, Qt::BlockingQueuedConnection);
        QVERIFY2(ok, "could not register the fake NetworkManager on the session bus");
        serviceRegistered_ = true;
    }

    void addWifi(quint32 state, const QString &ap, const QByteArray &ssid) {
        FakeDevice d;
        d.type = 2;
        d.state = state;
        d.activeAp = ap;
        d.ssid = ssid;
        QMutexLocker l(&fake_->mutex);
        fake_->devices << d;
    }
    int nextIndex() { QMutexLocker l(&fake_->mutex); return fake_->devices.size(); }

    QString makeNmcli(QTemporaryDir &dir, const QString &body) {
        const QString path = dir.filePath("nmcli");
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly)) return QString();
        f.write("#!/bin/sh\n" + body.toUtf8() + "\n");
        f.close();
        QFile::setPermissions(path, QFile::permissions(path) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
        return path;
    }

private slots:
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void cleanup() {
        if (!thread_) return;
        if (serviceRegistered_) {
            QMetaObject::invokeMethod(fake_, [&] {
                auto bus = QDBusConnection(QStringLiteral("fake-nm"));
                bus.unregisterObject(kNmPath);
                bus.unregisterService(kNmService);
            }, Qt::BlockingQueuedConnection);
            QDBusConnection::disconnectFromBus(QStringLiteral("fake-nm"));
            serviceRegistered_ = false;
        }
        thread_->quit();
        thread_->wait();
        delete fake_;
        delete thread_;
        fake_ = nullptr;
        thread_ = nullptr;
    }

    void connectedUtf8Ssid() {
        startFake(true);
        addWifi(100, FakeNetworkManager::apPath(0), QString::fromUtf8("Café ☕").toUtf8());
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString::fromUtf8("Café ☕"));
        QCOMPARE(got.source, QString("NetworkManager"));
    }

    void ethernetActiveWifiDisconnected() {
        startFake(true);
        {
            FakeDevice eth;
            eth.type = 1;
            eth.state = 100;
            QMutexLocker l(&fake_->mutex);
            fake_->devices << eth;
        }
        addWifi(30, "/", QByteArray());
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::NotConnected);
        QCOMPARE(got.source, QString("NetworkManager"));
    }

    void secondWifiDeviceConnected() {
        startFake(true);
        addWifi(30, "/", QByteArray());
        addWifi(100, FakeNetworkManager::apPath(1), "Office");
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString("Office"));
    }

    void latin1SsidFallsBack() {
        startFake(true);
        addWifi(100, FakeNetworkManager::apPath(0), QByteArray("\xE9t\xE9"));
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString::fromUtf8("été"));
    }

    void noWifiAdapter() {
        startFake(true);
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::NotConnected);
        QVERIFY(got.detail.contains("Wi-Fi adapter"));
    }

    void sandboxedWithoutServiceNeedsPermission() {
        startFake(false);
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", true);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::PermissionNeeded);
        QVERIFY2(got.detail.contains("flatpak override --user --system-talk-name=org.freedesktop.NetworkManager io.github.Ogstra.Proxor"),
                 qPrintable(got.detail));
    }

    void nmcliFallbackConnected() {
        if (IsWindowsHost()) QSKIP("fake nmcli is a shell script");
        startFake(false);
        QTemporaryDir dir;
        const QString nmcli = makeNmcli(dir, "printf 'no:Other\\nyes:Home\\\\:Net\\n'");
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), nmcli, false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString("Home:Net"));
        QCOMPARE(got.source, QString("nmcli"));
    }

    void nmcliMissingIsUnavailable() {
        startFake(false);
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY2(got.detail.contains("NetworkManager"), qPrintable(got.detail));
    }

    void nmcliFailureReportsStderr() {
        if (IsWindowsHost()) QSKIP("fake nmcli is a shell script");
        startFake(false);
        QTemporaryDir dir;
        const QString nmcli = makeNmcli(dir, "echo 'Error: NetworkManager is not running.' >&2\nexit 8");
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), nmcli, false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY2(got.detail.contains("Error: NetworkManager is not running."), qPrintable(got.detail));
    }

    void slowNetworkManagerStaysWithinBudget() {
        startFake(true);
        addWifi(100, FakeNetworkManager::apPath(0), "Slow");
        {
            QMutexLocker l(&fake_->mutex);
            fake_->delayMs = 3000;
        }
        NetworkManagerWifiReader r(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false, 600);
        QElapsedTimer t;
        t.start();
        const auto got = r.read();
        QVERIFY2(t.elapsed() < 600 + 1000, qPrintable(QString::number(t.elapsed())));
        QCOMPARE(got.state, ReadState::Unavailable);
        {
            QMutexLocker l(&fake_->mutex);
            fake_->delayMs = 0;
        }
    }
};

QTEST_MAIN(WifiNmTest)
#include "wifi_nm_test.moc"
