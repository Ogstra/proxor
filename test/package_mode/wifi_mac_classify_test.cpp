#include "platform/WifiMacClassify.hpp"

#include <QtTest>

using namespace ProxorWifi;

class WifiMacClassifyTest : public QObject {
    Q_OBJECT
private slots:
    void noInterface_data() { permissionRows(); }
    void noInterface() {
        QFETCH(bool, possible);
        MacWifiSnapshot s;
        const WifiReading r = ClassifyMacWifi(s, possible, "x");
        QCOMPARE(r.state, ReadState::Unavailable);
        QCOMPARE(r.detail, QString("This Mac has no Wi-Fi interface."));
        QCOMPARE(r.source, QString("CoreWLAN"));
    }
    void powerOff_data() { permissionRows(); }
    void powerOff() {
        QFETCH(bool, possible);
        MacWifiSnapshot s;
        s.hasInterface = true;
        const WifiReading r = ClassifyMacWifi(s, possible, "x");
        QCOMPARE(r.state, ReadState::NotConnected);
        QCOMPARE(r.detail, QString("Wi-Fi is turned off."));
        QCOMPARE(r.source, QString("CoreWLAN"));
    }
    void connected_data() { permissionRows(); }
    void connected() {
        QFETCH(bool, possible);
        MacWifiSnapshot s;
        s.hasInterface = true;
        s.powerOn = true;
        s.associated = true;
        s.ssid = "Home";
        const WifiReading r = ClassifyMacWifi(s, possible, "x");
        QCOMPARE(r.state, ReadState::Connected);
        QCOMPARE(r.ssid, QString("Home"));
        QCOMPARE(r.source, QString("CoreWLAN"));
    }
    void ssidWinsWhenNotAssociated() {
        MacWifiSnapshot s;
        s.hasInterface = true;
        s.powerOn = true;
        s.ssid = "Home";
        QCOMPARE(ClassifyMacWifi(s, true).state, ReadState::Connected);
    }
    void hiddenName() {
        MacWifiSnapshot s;
        s.hasInterface = true;
        s.powerOn = true;
        s.associated = true;
        const WifiReading r = ClassifyMacWifi(s, true);
        QCOMPARE(r.state, ReadState::PermissionNeeded);
        QVERIFY(DescribeReading(r).contains("Location Services"));
        QCOMPARE(r.source, QString("CoreWLAN"));
        const WifiReading u = ClassifyMacWifi(s, false, "Reason three");
        QCOMPARE(u.state, ReadState::Unavailable);
        QCOMPARE(u.detail, QString("Reason three"));
    }
    void notAssociated_data() { permissionRows(); }
    void notAssociated() {
        QFETCH(bool, possible);
        MacWifiSnapshot s;
        s.hasInterface = true;
        s.powerOn = true;
        const WifiReading r = ClassifyMacWifi(s, possible, "x");
        QCOMPARE(r.state, ReadState::NotConnected);
        QVERIFY(r.detail.isEmpty());
        QCOMPARE(r.source, QString("CoreWLAN"));
    }
    void mapping() {
        QCOMPARE(MapMacLocationStatus(0, true), PermissionState::NotDetermined);
        QCOMPARE(MapMacLocationStatus(1, true), PermissionState::Restricted);
        QCOMPARE(MapMacLocationStatus(2, true), PermissionState::Denied);
        QCOMPARE(MapMacLocationStatus(3, true), PermissionState::Granted);
        QCOMPARE(MapMacLocationStatus(4, true), PermissionState::Granted);
        QCOMPARE(MapMacLocationStatus(99, true), PermissionState::NotDetermined);
        for (int raw : {0, 2, 3, 99}) QCOMPARE(MapMacLocationStatus(raw, false), PermissionState::ServicesDisabled);
    }

private:
    static void permissionRows() {
        QTest::addColumn<bool>("possible");
        QTest::newRow("option1") << true;
        QTest::newRow("option3") << false;
    }
};

QTEST_GUILESS_MAIN(WifiMacClassifyTest)
#include "wifi_mac_classify_test.moc"
