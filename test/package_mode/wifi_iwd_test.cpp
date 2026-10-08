#include "fake_iwd.hpp"
#include "sys/wifi/WifiBackendIwd.hpp"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

using namespace ProxorWifi;

class WifiIwdTest : public QObject {
    Q_OBJECT
    QThread *thread_ = nullptr;
    FakeIwd *fake_ = nullptr;
    bool registered_ = false;
    const QString conn_ = QStringLiteral("fake-iwd");

    void startFake() {
        registered_ = StartFakeIwd(thread_, fake_, conn_);
        QVERIFY2(registered_, "could not register the fake iwd on the session bus");
    }

    // station i (0-based) with the given state; network "" = none. Adds the network when named.
    void addStation(const QString &state, const QString &networkName) {
        QMutexLocker l(&fake_->mutex);
        FakeIwdStation s;
        s.state = state;
        if (!networkName.isEmpty()) {
            const QString path = FakeIwd::networkPath(fake_->networks.size());
            fake_->networks.insert(path, FakeIwdNetwork{networkName});
            s.network = path;
        }
        fake_->stations << s;
    }

private slots:
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void cleanup() {
        StopFakeIwd(thread_, fake_, conn_, registered_);
        registered_ = false;
    }

    void connectedUtf8Name() {
        startFake();
        addStation("connected", QString::fromUtf8("Café ☕"));
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString::fromUtf8("Café ☕"));
        QCOMPARE(got.source, QString("iwd"));
        QVERIFY(r.presence() == IwdPresence::Present);
    }

    void roamingCountsAsConnected() {
        startFake();
        addStation("roaming", "Roam");
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString("Roam"));
    }

    void connectingIsNotConnected() {
        startFake();
        addStation("connecting", "Pending");
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::NotConnected);
        QVERIFY(r.presence() == IwdPresence::Present);
    }

    void disconnectedStation() {
        startFake();
        addStation("disconnected", QString());
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::NotConnected);
        QVERIFY(got.detail.isEmpty());
        QCOMPARE(DescribeReading(got), QString("Not connected to a Wi-Fi network."));
    }

    void secondStationConnected() {
        startFake();
        addStation("disconnected", QString());
        addStation("connected", "Office");
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString("Office"));
    }

    void deviceWithoutStation() {
        startFake();
        {
            QMutexLocker l(&fake_->mutex);
            fake_->extraDevicesWithoutStation = 1;
        }
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::NotConnected);
        QVERIFY(got.detail.isEmpty());
        QVERIFY(r.presence() == IwdPresence::Present);
    }

    void noAdapter() {
        startFake();
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::NotConnected);
        QCOMPARE(got.detail, QString("No Wi-Fi adapter found."));
        QVERIFY(r.presence() == IwdPresence::Present);
    }

    void iwdAbsentIsUnreachableAndFast() {
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        QElapsedTimer t;
        t.start();
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY(r.presence() == IwdPresence::Unreachable);
        QVERIFY(got.detail.isEmpty());
        QVERIFY2(t.elapsed() < 1000, "absent iwd must return at once");
    }

    void accessDeniedIsNeutral() {
        startFake();
        {
            QMutexLocker l(&fake_->mutex);
            fake_->denied = true;
        }
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY(r.presence() == IwdPresence::Denied);
        QVERIFY(got.detail.isEmpty());
    }

    void neverAutoStartsIwd() {
        startFake();
        addStation("connected", "Home");
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        r.read();
        QMutexLocker l(&fake_->mutex);
        // One call only. QtDBus does not expose the NO_AUTO_START header flag on the receiving side
        // (QDBusMessage::autoStartService() stays at its default there), so the flag itself is pinned by
        // setAutoStartService(false) in the reader source (wiring guard + phase proof check 13) and was
        // checked on the wire with a libdbus receiver (dbus_message_get_auto_start() == 0).
        QCOMPARE(fake_->calls, 1);
    }

    void disconnectedBusIsUnreachable() {
        IwdWifiReader r(QDBusConnection(QStringLiteral("proxor-not-connected")), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY(r.presence() == IwdPresence::Unreachable);
        QVERIFY(got.detail.isEmpty());
    }

    void slowIwdStaysWithinBudget() {
        startFake();
        addStation("connected", "Slow");
        {
            QMutexLocker l(&fake_->mutex);
            fake_->delayMs = 3000;
        }
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        QElapsedTimer t;
        t.start();
        const auto got = r.readWithin(600);
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY(r.presence() == IwdPresence::TimedOut);
        QVERIFY(got.detail.isEmpty());
        QVERIFY2(t.elapsed() < 1600, "a slow iwd must not exceed the budget");
    }

    void malformedReplyIsUnavailable() {
        startFake();
        {
            QMutexLocker l(&fake_->mutex);
            fake_->malformed = true;
        }
        IwdWifiReader r(QDBusConnection::sessionBus(), false);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY(r.presence() == IwdPresence::Failed);
        QVERIFY(got.detail.isEmpty());
    }

    void sandboxedNeverCallsTheBus() {
        startFake();
        addStation("connected", "Home");
        IwdWifiReader r(QDBusConnection::sessionBus(), true);
        const auto got = r.read();
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY(r.presence() == IwdPresence::Sandboxed);
        QVERIFY(got.detail.isEmpty());
        QMutexLocker l(&fake_->mutex);
        QCOMPARE(fake_->calls, 0);
    }
};

QTEST_MAIN(WifiIwdTest)
#include "wifi_iwd_test.moc"
