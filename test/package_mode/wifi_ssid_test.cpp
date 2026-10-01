#include "platform/WifiSsid.hpp"

#include <QtTest>

using namespace ProxorWifi;

class WifiSsidTest : public QObject {
    Q_OBJECT
private slots:
    void netshConnected() {
        const QString out = "    Name                   : Wi-Fi\n    State                  : connected\n"
                            "    SSID                   : Home Net\n    BSSID                  : aa:bb:cc:dd:ee:ff\n";
        const auto r = ParseNetshInterfaces(out);
        QCOMPARE(r.state, ReadState::Connected);
        QCOMPARE(r.ssid, QString("Home Net"));
        QCOMPARE(r.source, QString("netsh"));
    }
    void netshColonInSsid() {
        const auto r = ParseNetshInterfaces("    SSID                   : Cafe: 2\n");
        QCOMPARE(r.state, ReadState::Connected);
        QCOMPARE(r.ssid, QString("Cafe: 2"));
    }
    void netshNotConnected() {
        QCOMPARE(ParseNetshInterfaces("    State : disconnected\n").state, ReadState::NotConnected);
        const auto none = ParseNetshInterfaces("There is no wireless interface on the system.\n");
        QCOMPARE(none.state, ReadState::NotConnected);
        QVERIFY(none.detail.contains("adapter", Qt::CaseInsensitive));
        QCOMPARE(ParseNetshInterfaces("    SSID                   : \n").state, ReadState::NotConnected);
    }
    void netshPermission() {
        const auto r = ParseNetshInterfaces("Access is denied. Needs Location Permission to show interfaces.\n");
        QCOMPARE(r.state, ReadState::PermissionNeeded);
        QVERIFY(r.detail.contains("Privacy & security"));
        QVERIFY(r.detail.contains("Location"));
    }
    void nmcli() {
        auto r = ParseNmcliWifiList("no:Other\nyes:Home Net\n");
        QCOMPARE(r.state, ReadState::Connected);
        QCOMPARE(r.ssid, QString("Home Net"));
        QCOMPARE(r.source, QString("nmcli"));
        QCOMPARE(ParseNmcliWifiList("yes:My\\:Net\n").ssid, QString("My:Net"));
        QCOMPARE(ParseNmcliWifiList("yes:Back\\\\slash\n").ssid, QString("Back\\slash"));
        QCOMPARE(ParseNmcliWifiList("no:A\nno:B\n").state, ReadState::NotConnected);
        QCOMPARE(ParseNmcliWifiList("").state, ReadState::NotConnected);
        r = ParseNmcliWifiList("yes:\n");
        QCOMPARE(r.state, ReadState::NotConnected);
        QVERIFY(r.detail.contains("hidden", Qt::CaseInsensitive));
    }
    void decode() {
        const QString cafe = QString::fromUtf8("Caf\xc3\xa9 \xe2\x98\x95");
        QCOMPARE(DecodeSsidBytes(cafe.toUtf8()), cafe);
        QCOMPARE(DecodeSsidBytes(QByteArray("\xe9\x74\xe9", 3)), QString::fromUtf8("\xc3\xa9t\xc3\xa9"));
        QCOMPARE(DecodeSsidBytes(QByteArray("abc\0\0", 5)), QString("abc"));
        QCOMPARE(DecodeSsidBytes(QByteArray()), QString());
    }
    void monitoringNeeded() {
        QVERIFY(MonitoringNeeded(true, {"Home"}, ""));
        QVERIFY(!MonitoringNeeded(true, {}, ""));
        QVERIFY(!MonitoringNeeded(false, {"Home"}, ""));
        QVERIFY(MonitoringNeeded(false, {}, "a.lan 10.0.0.1 Home"));
        QVERIFY(!MonitoringNeeded(false, {}, "#a.lan 10.0.0.1 Home"));
        QVERIFY(!MonitoringNeeded(false, {}, "a.lan 10.0.0.1"));
        QVERIFY(!MonitoringNeeded(false, {}, "a.lan 10.0.0.1 ,,"));
    }
    void hostsSkip() {
        const QString m = "a.lan 10.0.0.1 Home,Office";
        QVERIFY(HostsSkipDiffers(m, "", "Home"));
        QVERIFY(!HostsSkipDiffers(m, "Home", "Office"));
        QVERIFY(HostsSkipDiffers(m, "Home", "Cafe"));
        QVERIFY(!HostsSkipDiffers(m, "Cafe", "Bar"));
        QVERIFY(!HostsSkipDiffers(m, "Home", "Home"));
        QVERIFY(!HostsSkipDiffers("a.lan 10.0.0.1", "", "Home"));
        QVERIFY(HostsSkipDiffers("a.lan 10.0.0.1  Home", "", "Home"));
        QVERIFY(!HostsSkipDiffers(m, "", "home"));
    }
    void changeTracker() {
        ChangeTracker t;
        auto u = t.apply(NotConnected("", "test"));
        QVERIFY(!u.ssidChanged);
        QVERIFY(u.readingChanged);
        QCOMPARE(u.ssid, QString());
        u = t.apply(Connected("A", "test"));
        QVERIFY(u.ssidChanged);
        QCOMPARE(u.ssid, QString("A"));
        u = t.apply(Connected("A", "test"));
        QVERIFY(!u.ssidChanged);
        QVERIFY(!u.readingChanged);
        u = t.apply(Unavailable("x", "test"));
        QVERIFY(!u.ssidChanged);
        QVERIFY(u.readingChanged);
        QCOMPARE(t.ssid(), QString("A"));
        u = t.apply(PermissionNeeded("y", "test"));
        QVERIFY(!u.ssidChanged);
        QVERIFY(u.readingChanged);
        QCOMPARE(t.ssid(), QString("A"));
        u = t.apply(NotConnected("", "test"));
        QVERIFY(u.ssidChanged);
        QCOMPARE(t.ssid(), QString());
        t.apply(Connected("A", "test"));
        u = t.apply(Connected("", "test"));
        QVERIFY(u.ssidChanged);
        QCOMPARE(t.ssid(), QString());
        QCOMPARE(t.last().state, ReadState::NotConnected);
    }
    void describeReading() {
        QVERIFY(DescribeReading(Connected("Home", "netsh")).contains("\"Home\""));
        QVERIFY(DescribeReading(Unavailable("because", "x")).contains("because"));
        QVERIFY(!DescribeReading(Unavailable("", "x")).isEmpty());
        QVERIFY(!DescribeReading(NotConnected("", "x")).isEmpty());
        QVERIFY(!DescribeReading(PermissionNeeded("", "x")).isEmpty());
        QCOMPARE(DescribeReading(NotConnected("nope", "x")), QString("nope"));
    }
    void describePermission() {
        QVERIFY(DescribePermission(PermissionState::NotRequired).isEmpty());
        QVERIFY(DescribePermission(PermissionState::Granted).isEmpty());
        QVERIFY(!DescribePermission(PermissionState::NotDetermined).isEmpty());
        QVERIFY(DescribePermission(PermissionState::Denied).contains("Location Services"));
        QVERIFY(!DescribePermission(PermissionState::Restricted).isEmpty());
        QVERIFY(DescribePermission(PermissionState::ServicesDisabled).contains("turned off"));
    }
    void permissionPrompt() {
        for (auto s : {PermissionState::NotRequired, PermissionState::NotDetermined, PermissionState::Granted,
                       PermissionState::Denied, PermissionState::Restricted, PermissionState::ServicesDisabled})
            QCOMPARE(DecidePermissionPrompt(s, false, false), PermissionPrompt::None);
        QCOMPARE(DecidePermissionPrompt(PermissionState::NotDetermined, true, false), PermissionPrompt::ExplainThenRequest);
        QCOMPARE(DecidePermissionPrompt(PermissionState::NotDetermined, true, true), PermissionPrompt::None);
        for (auto s : {PermissionState::Denied, PermissionState::Restricted, PermissionState::ServicesDisabled})
            QCOMPARE(DecidePermissionPrompt(s, true, false), PermissionPrompt::PointToSettings);
        QCOMPARE(DecidePermissionPrompt(PermissionState::NotRequired, true, false), PermissionPrompt::None);
        QCOMPARE(DecidePermissionPrompt(PermissionState::Granted, true, false), PermissionPrompt::None);
    }
};

QTEST_GUILESS_MAIN(WifiSsidTest)
#include "wifi_ssid_test.moc"
