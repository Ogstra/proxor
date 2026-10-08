#include "platform/TunAddress.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace ProxorPlatform;

class TunAddressTest : public QObject {
    Q_OBJECT

    static void put(const QString &root, const QString &rel, const QByteArray &data) {
        const QString path = QDir(root).filePath(rel);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(data);
    }

private slots:
    void addressList() {
        QCOMPARE(TunAddresses(false), QStringList{"172.19.0.1/28"});
        QCOMPARE(TunAddresses(true), (QStringList{"172.19.0.1/28", "fdfe:dcba:9876::1/126"}));
    }

    void nonLinuxPassesThrough_data() {
        QTest::addColumn<int>("os");
        QTest::addColumn<int>("state");
        for (int os : {int(HostOs::Windows), int(HostOs::MacOS), int(HostOs::Other)})
            for (int st : {int(Ipv6KernelState::Available), int(Ipv6KernelState::Disabled), int(Ipv6KernelState::Unknown)})
                QTest::newRow(qPrintable(QString("os%1_state%2").arg(os).arg(st))) << os << st;
    }
    void nonLinuxPassesThrough() {
        QFETCH(int, os);
        QFETCH(int, state);
        QCOMPARE(EffectiveTunIpv6(HostOs(os), true, Ipv6KernelState(state)), true);
        QCOMPARE(EffectiveTunIpv6(HostOs(os), false, Ipv6KernelState(state)), false);
    }

    void linuxPolicy() {
        QCOMPARE(EffectiveTunIpv6(HostOs::Linux, true, Ipv6KernelState::Available), true);
        QCOMPARE(EffectiveTunIpv6(HostOs::Linux, true, Ipv6KernelState::Unknown), true);
        QCOMPARE(EffectiveTunIpv6(HostOs::Linux, true, Ipv6KernelState::Disabled), false);
        QCOMPARE(EffectiveTunIpv6(HostOs::Linux, false, Ipv6KernelState::Available), false);
        QCOMPARE(EffectiveTunIpv6(HostOs::Linux, false, Ipv6KernelState::Disabled), false);
        QCOMPARE(EffectiveTunIpv6(HostOs::Linux, false, Ipv6KernelState::Unknown), false);
    }

    void procAvailable() {
        QTemporaryDir d;
        put(d.path(), "proc/net/if_inet6", "x\n");
        put(d.path(), "proc/sys/net/ipv6/conf/all/disable_ipv6", "0\n");
        put(d.path(), "proc/sys/net/ipv6/conf/default/disable_ipv6", "0\n");
        QCOMPARE(ReadIpv6KernelState(d.path()), Ipv6KernelState::Available);
    }
    void procNoIfInet6() {
        QTemporaryDir d;
        put(d.path(), "proc/sys/net/ipv6/conf/all/disable_ipv6", "0\n");
        QCOMPARE(ReadIpv6KernelState(d.path()), Ipv6KernelState::Disabled);
    }
    void procAllDisabled() {
        QTemporaryDir d;
        put(d.path(), "proc/net/if_inet6", "x\n");
        put(d.path(), "proc/sys/net/ipv6/conf/all/disable_ipv6", "1\n");
        put(d.path(), "proc/sys/net/ipv6/conf/default/disable_ipv6", "0\n");
        QCOMPARE(ReadIpv6KernelState(d.path()), Ipv6KernelState::Disabled);
    }
    void procDefaultDisabled() {
        QTemporaryDir d;
        put(d.path(), "proc/net/if_inet6", "x\n");
        put(d.path(), "proc/sys/net/ipv6/conf/all/disable_ipv6", "0\n");
        put(d.path(), "proc/sys/net/ipv6/conf/default/disable_ipv6", "1");
        QCOMPARE(ReadIpv6KernelState(d.path()), Ipv6KernelState::Disabled);
    }
    void procFlagsMissing() {
        QTemporaryDir d;
        put(d.path(), "proc/net/if_inet6", "x\n");
        QCOMPARE(ReadIpv6KernelState(d.path()), Ipv6KernelState::Available);
    }
    void procRootMissing() {
        QTemporaryDir d;
        QCOMPARE(ReadIpv6KernelState(QDir(d.path()).filePath("nope")), Ipv6KernelState::Unknown);
    }

    void noticeText() {
        const QString n = TunIpv6DroppedNotice();
        QVERIFY(!n.isEmpty());
        QVERIFY(n.contains("IPv6"));
    }
};

QTEST_GUILESS_MAIN(TunAddressTest)
#include "tun_address_test.moc"
