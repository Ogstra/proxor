#include <QtTest>

#include "platform/LocalNetworks.hpp"

using namespace ProxorPlatform;

namespace {
quint32 Ip(int a, int b, int c, int d) {
    return (static_cast<quint32>(a) << 24) | (static_cast<quint32>(b) << 16) | (static_cast<quint32>(c) << 8) |
           static_cast<quint32>(d);
}
} // namespace

class LocalNetworksTest : public QObject {
    Q_OBJECT
private slots:
    void privateNetworkCidr() {
        QCOMPARE(PrivateNetworkCidr({Ip(192, 168, 1, 37), 24}), QStringLiteral("192.168.1.0/24"));
        QCOMPARE(PrivateNetworkCidr({Ip(10, 20, 30, 40), 8}), QStringLiteral("10.0.0.0/8"));
        QCOMPARE(PrivateNetworkCidr({Ip(172, 16, 5, 9), 12}), QStringLiteral("172.16.0.0/12"));
        QCOMPARE(PrivateNetworkCidr({Ip(172, 31, 255, 1), 16}), QStringLiteral("172.31.0.0/16"));
        QCOMPARE(PrivateNetworkCidr({Ip(10, 8, 0, 2), 32}), QStringLiteral("10.8.0.2/32"));
    }
    void notPrivateIsEmpty() {
        QVERIFY(PrivateNetworkCidr({Ip(8, 8, 8, 8), 24}).isEmpty());
        QVERIFY(PrivateNetworkCidr({Ip(172, 32, 0, 1), 16}).isEmpty());   // just outside 172.16/12
        QVERIFY(PrivateNetworkCidr({Ip(100, 64, 0, 1), 10}).isEmpty());   // CGNAT stays a separate rule
        QVERIFY(PrivateNetworkCidr({Ip(169, 254, 1, 1), 16}).isEmpty());  // link-local stays a separate rule
        QVERIFY(PrivateNetworkCidr({Ip(127, 0, 0, 1), 8}).isEmpty());
        QVERIFY(PrivateNetworkCidr({Ip(192, 168, 1, 1), 0}).isEmpty());   // a /0 must never exclude everything
        QVERIFY(PrivateNetworkCidr({Ip(192, 168, 1, 1), 33}).isEmpty());
    }
    void attachedAreSortedAndDeduplicated() {
        const QList<Ipv4Net> ifaces{{Ip(192, 168, 1, 10), 24}, {Ip(10, 0, 0, 5), 24}, {Ip(192, 168, 1, 99), 24}, {Ip(8, 8, 4, 4), 24}};
        QCOMPARE(PrivateAttachedNetworks(ifaces), (QStringList{QStringLiteral("10.0.0.0/24"), QStringLiteral("192.168.1.0/24")}));
    }
    void ignoredRangeIsDropped() {
        const QList<Ipv4Net> ifaces{{Ip(172, 19, 0, 1), 28}, {Ip(172, 19, 0, 1), 32}, {Ip(172, 20, 0, 1), 24}};
        QCOMPARE(PrivateAttachedNetworks(ifaces, {QStringLiteral("172.19.0.0/28")}), (QStringList{QStringLiteral("172.20.0.0/24")}));
    }
    void remoteNetworkBehindProxyIsNotExcluded() {
        // The case behind this module: a home LAN of 192.168.1.0/24 must not keep 10.10.10.0/24 local.
        const auto attached = PrivateAttachedNetworks({{Ip(192, 168, 1, 10), 24}});
        QVERIFY(!attached.contains(QStringLiteral("10.0.0.0/8")));
        QVERIFY(!attached.contains(QStringLiteral("10.10.10.0/24")));
    }
    void replaceBlanketRanges() {
        const QStringList base{QStringLiteral("127.0.0.1"), QStringLiteral("localhost"), QStringLiteral("*.local"),
                               QStringLiteral("169.254.0.0/16"), QStringLiteral("10.0.0.0/8"), QStringLiteral("172.16.0.0/12"),
                               QStringLiteral("192.168.0.0/16"), QStringLiteral("100.64.0.0/10")};
        const QStringList want{QStringLiteral("127.0.0.1"), QStringLiteral("localhost"), QStringLiteral("*.local"),
                               QStringLiteral("169.254.0.0/16"), QStringLiteral("100.64.0.0/10"), QStringLiteral("192.168.1.0/24")};
        QCOMPARE(ReplaceBlanketPrivateRanges(base, {QStringLiteral("192.168.1.0/24")}), want);
        // With nothing attached the blanket ranges are simply gone.
        QVERIFY(!ReplaceBlanketPrivateRanges(base, {}).contains(QStringLiteral("10.0.0.0/8")));
    }
};

QTEST_MAIN(LocalNetworksTest)
#include "local_networks_test.moc"
