#include "fake_iwd.hpp"
#include "sys/wifi/WifiBackendChain.hpp"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QThread>
#include <QtTest>

using namespace ProxorWifi;

namespace {

WifiReading Reading(ReadState state, const QString &ssid, const QString &detail, const QString &source) {
    WifiReading r;
    r.state = state;
    r.ssid = ssid;
    r.detail = detail;
    r.source = source;
    return r;
}

void VerifySame(const WifiReading &a, const WifiReading &b) {
    QCOMPARE(a.state, b.state);
    QCOMPARE(a.ssid, b.ssid);
    QCOMPARE(a.detail, b.detail);
    QCOMPARE(a.source, b.source);
}

} // namespace

class WifiChainTest : public QObject {
    Q_OBJECT
    QThread *thread_ = nullptr;
    FakeIwd *fake_ = nullptr;
    bool registered_ = false;
    const QString conn_ = QStringLiteral("fake-iwd-chain");

    void startFake() {
        registered_ = StartFakeIwd(thread_, fake_, conn_);
        QVERIFY2(registered_, "could not register the fake iwd on the session bus");
    }

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
    void cleanup() {
        StopFakeIwd(thread_, fake_, conn_, registered_);
        registered_ = false;
    }

    // ---- pure choice table (no bus) ----

    void managesWifiNeverAsksIwd() {
        const WifiReading nm = Reading(ReadState::Connected, "Office", QString(), "NetworkManager");
        for (NmWifiPresence p : {NmWifiPresence::ManagesWifi, NmWifiPresence::TimedOut, NmWifiPresence::Failed}) {
            int calls = 0;
            const auto got = ChooseWifiReading(nm, p, [&] {
                ++calls;
                return std::make_pair(Reading(ReadState::Connected, "Home", QString(), "iwd"), IwdPresence::Present);
            });
            VerifySame(got, nm);
            QCOMPARE(calls, 0);
        }
    }

    void iwdUsedWhenNmAbsent() {
        const WifiReading nm = Reading(ReadState::Unavailable, QString(), "gone", "nmcli");
        int calls = 0;
        const auto got = ChooseWifiReading(nm, NmWifiPresence::Unreachable, [&] {
            ++calls;
            return std::make_pair(Reading(ReadState::Connected, "Home", QString(), "iwd"), IwdPresence::Present);
        });
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString("Home"));
        QCOMPARE(got.source, QString("iwd"));
        QCOMPARE(calls, 1);
    }

    void iwdUsedWhenNmHasNoWifiDevice() {
        const WifiReading nm = Reading(ReadState::NotConnected, QString(), "No Wi-Fi adapter found.", "NetworkManager");
        int calls = 0;
        const auto got = ChooseWifiReading(nm, NmWifiPresence::NoWifiDevice, [&] {
            ++calls;
            return std::make_pair(Reading(ReadState::Connected, "Home", QString(), "iwd"), IwdPresence::Present);
        });
        QCOMPARE(got.source, QString("iwd"));
        QCOMPARE(calls, 1);
    }

    void iwdUsedWhenNmDoesNotManageWifi() {
        const WifiReading nm = Reading(ReadState::Unavailable, QString(), "unmanaged", "NetworkManager");
        int calls = 0;
        const auto got = ChooseWifiReading(nm, NmWifiPresence::NotManagingWifi, [&] {
            ++calls;
            return std::make_pair(Reading(ReadState::NotConnected, QString(), QString(), "iwd"), IwdPresence::Present);
        });
        QCOMPARE(got.state, ReadState::NotConnected);
        QCOMPARE(got.source, QString("iwd"));
        QCOMPARE(calls, 1);
    }

    void iwdNotAnsweringKeepsNmReading() {
        const WifiReading nm = Reading(ReadState::Unavailable, QString(), "nm detail", "nmcli");
        for (IwdPresence p : {IwdPresence::Unreachable, IwdPresence::Denied, IwdPresence::TimedOut, IwdPresence::Failed, IwdPresence::Sandboxed}) {
            int calls = 0;
            const auto got = ChooseWifiReading(nm, NmWifiPresence::Unreachable, [&] {
                ++calls;
                return std::make_pair(Reading(ReadState::Unavailable, QString(), QString(), "iwd"), p);
            });
            VerifySame(got, nm);
            QCOMPARE(calls, 1);
        }
    }

    void flatpakKeepsTheNmPermissionHint() {
        const WifiReading nm = Reading(ReadState::PermissionNeeded, QString(), "The Flatpak cannot reach NetworkManager", "NetworkManager");
        const auto got = ChooseWifiReading(nm, NmWifiPresence::Unreachable, [&] {
            return std::make_pair(Reading(ReadState::Unavailable, QString(), QString(), "iwd"), IwdPresence::Sandboxed);
        });
        VerifySame(got, nm);
    }

    // ---- chain on a private bus ----

    void chainReadsIwdWhenNmIsAbsent() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
        startFake();
        addStation("connected", QString::fromUtf8("Café"));
        NmThenIwdWifiReader chain(
            std::make_unique<NetworkManagerWifiReader>(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false),
            std::make_unique<IwdWifiReader>(QDBusConnection::sessionBus(), false));
        const auto got = chain.read();
        QCOMPARE(got.state, ReadState::Connected);
        QCOMPARE(got.ssid, QString::fromUtf8("Café"));
        QCOMPARE(got.source, QString("iwd"));
    }

    void chainBothAbsentExplainsAndIsFast() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
        NmThenIwdWifiReader chain(
            std::make_unique<NetworkManagerWifiReader>(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false),
            std::make_unique<IwdWifiReader>(QDBusConnection::sessionBus(), false));
        QElapsedTimer t;
        t.start();
        const auto got = chain.read();
        QVERIFY2(t.elapsed() < 1500, qPrintable(QString::number(t.elapsed())));
        QCOMPARE(got.state, ReadState::Unavailable);
        QVERIFY2(got.detail.contains("NetworkManager or iwd"), qPrintable(got.detail));
    }

    void chainStaysWithinTotalBudget() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
        startFake();
        addStation("connected", "Slow");
        {
            QMutexLocker l(&fake_->mutex);
            fake_->delayMs = 3000;
        }
        NmThenIwdWifiReader chain(
            std::make_unique<NetworkManagerWifiReader>(QDBusConnection::sessionBus(), "/nonexistent/nmcli", false),
            std::make_unique<IwdWifiReader>(QDBusConnection::sessionBus(), false), 1200);
        QElapsedTimer t;
        t.start();
        const auto got = chain.read();
        QVERIFY2(t.elapsed() < 2200, qPrintable(QString::number(t.elapsed())));
        QCOMPARE(got.state, ReadState::Unavailable);
        QCOMPARE(got.source, QString("nmcli"));
        {
            QMutexLocker l(&fake_->mutex);
            fake_->delayMs = 0;
        }
    }
};

QTEST_MAIN(WifiChainTest)
#include "wifi_chain_test.moc"
