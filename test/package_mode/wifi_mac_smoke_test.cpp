// Real CoreWLAN read. Asserts shape only and prints the state name, never the network name.
#include "sys/wifi/WifiBackend.hpp"

#include <QElapsedTimer>
#include <QtTest>

class WifiMacSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void readsOnce() {
        auto backend = CreatePlatformWifiBackend();
        QVERIFY(backend);
        QElapsedTimer timer;
        timer.start();
        const ProxorWifi::WifiReading r = backend->read();
        QVERIFY(timer.elapsed() < 4000);
        QCOMPARE(r.source, QString("CoreWLAN"));
        if (r.state == ProxorWifi::ReadState::Connected) QVERIFY(!r.ssid.isEmpty());
        const char *name = "Unavailable";
        switch (r.state) {
            case ProxorWifi::ReadState::Connected: name = "Connected"; break;
            case ProxorWifi::ReadState::NotConnected: name = "NotConnected"; break;
            case ProxorWifi::ReadState::PermissionNeeded: name = "PermissionNeeded"; break;
            case ProxorWifi::ReadState::Unavailable: name = "Unavailable"; break;
        }
        qInfo("wifi_mac_smoke state=%s", name);
    }
};

QTEST_GUILESS_MAIN(WifiMacSmokeTest)
#include "wifi_mac_smoke_test.moc"
