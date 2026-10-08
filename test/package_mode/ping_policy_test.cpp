#include <QtTest>

#include "platform/PingPolicy.hpp"

using namespace ProxorPlatform;

class PingPolicyTest : public QObject {
    Q_OBJECT
private slots:
    void localFailureDetection() {
        QVERIFY(IsLocalIcmpFailure("icmp-unavailable: udp4: permission denied"));
        QVERIFY(!IsLocalIcmpFailure(""));
        QVERIFY(!IsLocalIcmpFailure("i/o timeout"));
        QVERIFY(!IsLocalIcmpFailure("ICMP-UNAVAILABLE: x"));
        QVERIFY(!IsLocalIcmpFailure(" icmp-unavailable: x"));
    }
    void classify() {
        QCOMPARE(ClassifyPingResult(3, ""), PingOutcome::Latency);
        QCOMPARE(ClassifyPingResult(3, "icmp-unavailable: x"), PingOutcome::RetryWithTcp);
        QCOMPARE(ClassifyPingResult(3, "read ip4 0.0.0.0: i/o timeout"), PingOutcome::Unavailable);
        QCOMPARE(ClassifyPingResult(0, "icmp-unavailable: x"), PingOutcome::Unavailable);
        QCOMPARE(ClassifyPingResult(1, ""), PingOutcome::Latency);
        QCOMPARE(ClassifyPingResult(4, "timeout"), PingOutcome::Unavailable);
    }
    void effectiveMode() {
        QCOMPARE(EffectivePingMode(3, true), 0);
        QCOMPARE(EffectivePingMode(3, false), 3);
        QCOMPARE(EffectivePingMode(0, true), 0);
        QCOMPARE(EffectivePingMode(1, true), 1);
        QCOMPARE(EffectivePingMode(4, true), 4);
    }
    void notice() {
        const auto n = IcmpFallbackNotice("icmp-unavailable: udp4: operation not permitted");
        QVERIFY(n.contains("udp4: operation not permitted"));
        QVERIFY(!n.contains("icmp-unavailable: "));
        QVERIFY(n.contains("TCP"));
    }
};

QTEST_GUILESS_MAIN(PingPolicyTest)
#include "ping_policy_test.moc"
