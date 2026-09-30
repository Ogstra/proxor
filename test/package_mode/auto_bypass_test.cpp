#include "platform/AutoBypass.hpp"

#include <QtTest>

using namespace ProxorPlatform;

class AutoBypassTest : public QObject {
    Q_OBJECT
private slots:
    void vpnClientNames() {
        QCOMPARE(KnownVpnClientProcessNames(HostOs::Windows), (QStringList{"wireguard.exe", "openvpn.exe", "tailscaled.exe"}));
        QCOMPARE(KnownVpnClientProcessNames(HostOs::Linux), (QStringList{"wireguard-go", "openvpn", "tailscaled"}));
        QCOMPARE(KnownVpnClientProcessNames(HostOs::MacOS), (QStringList{"wireguard-go", "openvpn", "tailscaled", "WireGuard"}));
        QVERIFY(KnownVpnClientProcessNames(HostOs::Other).isEmpty());
    }
    void windows() {
        auto r = BuildAutoBypassProcesses({"C:/cores/hysteria2.exe", "C:\\cores\\tuic.exe", "naive.exe", "", "  "}, HostOs::Windows);
        QCOMPARE(r.processPaths, (QStringList{"C:\\cores\\hysteria2.exe", "C:\\cores\\tuic.exe"}));
        QCOMPARE(r.processNames, (QStringList{"hysteria2.exe", "tuic.exe", "naive.exe", "wireguard.exe", "openvpn.exe", "tailscaled.exe"}));
    }
    void windowsDedupeCaseInsensitive() {
        auto r = BuildAutoBypassProcesses({"C:\\a\\X.exe", "c:/a/x.exe"}, HostOs::Windows);
        QCOMPARE(r.processPaths.size(), 1);
        QCOMPARE(r.processNames.count("X.exe"), 1);
        QCOMPARE(r.processNames.size(), 4);
    }
    void linux() {
        auto r = BuildAutoBypassProcesses({"/opt/cores/hysteria2", "naive"}, HostOs::Linux);
        QCOMPARE(r.processPaths, (QStringList{"/opt/cores/hysteria2"}));
        QCOMPARE(r.processNames, (QStringList{"hysteria2", "naive", "wireguard-go", "openvpn", "tailscaled"}));
        r = BuildAutoBypassProcesses({"/a/Core", "/b/core"}, HostOs::Linux);
        QCOMPARE(r.processPaths.size(), 2);
        QCOMPARE(r.processNames.mid(0, 2), (QStringList{"Core", "core"}));
    }
    void macos() {
        auto r = BuildAutoBypassProcesses({"/Applications/Proxor.app/Contents/MacOS/hysteria2"}, HostOs::MacOS);
        QCOMPARE(r.processPaths, (QStringList{"/Applications/Proxor.app/Contents/MacOS/hysteria2"}));
        QCOMPARE(r.processNames.first(), QString("hysteria2"));
    }
    void relativeWithSeparatorIsNameOnly() {
        auto r = BuildAutoBypassProcesses({"cores/hysteria2"}, HostOs::Linux);
        QVERIFY(r.processPaths.isEmpty());
        QCOMPARE(r.processNames.first(), QString("hysteria2"));
    }
    void noExeOnUnix() {
        for (auto os : {HostOs::Linux, HostOs::MacOS}) {
            auto r = BuildAutoBypassProcesses({}, os);
            QVERIFY(!r.processNames.isEmpty());
            for (const auto &n : r.processNames) QVERIFY(!n.endsWith(".exe"));
        }
    }
};

QTEST_APPLESS_MAIN(AutoBypassTest)
#include "auto_bypass_test.moc"
