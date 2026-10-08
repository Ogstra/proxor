// macOS sleep/wake observer smoke test: notifications are posted in-process, the Mac never sleeps.
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QtTest>
#include <atomic>
#include <thread>
#include "sys/SleepWake.hpp"

void ProxorTestPostWorkspaceSleepWake(bool);

class MacSleepWakeSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void installReportsNSWorkspace() {
        QObject owner;
        auto r = ProxorSleepWake::Install(&owner, [](bool) {});
        QVERIFY(r.installed);
        QVERIFY(r.detail.contains(QStringLiteral("NSWorkspace")));
    }

    void mainThreadDeliveryIsSynchronous() {
        QObject owner;
        QList<bool> events;
        QVERIFY(ProxorSleepWake::Install(&owner, [&](bool s) { events << s; }).installed);
        ProxorTestPostWorkspaceSleepWake(true);
        QCOMPARE(events.size(), 1);
        QCOMPARE(events.at(0), true);
        ProxorTestPostWorkspaceSleepWake(false);
        QCOMPARE(events.size(), 2);
        QCOMPARE(events.at(1), false);
    }

    void crossThreadPostIsQueuedToOwnerThread() {
        QObject owner;
        QList<bool> events;
        QThread *seen = nullptr;
        QVERIFY(ProxorSleepWake::Install(&owner, [&](bool s) {
                    events << s;
                    seen = QThread::currentThread();
                }).installed);
        std::thread t([] { ProxorTestPostWorkspaceSleepWake(true); });
        t.join();
        QTRY_COMPARE(events.size(), 1);
        QCOMPARE(events.at(0), true);
        QCOMPARE(seen, owner.thread());
    }

    void noCallAfterOwnerDeleted() {
        int calls = 0;
        auto *owner = new QObject;
        QVERIFY(ProxorSleepWake::Install(owner, [&](bool) { ++calls; }).installed);
        delete owner;
        ProxorTestPostWorkspaceSleepWake(true);
        ProxorTestPostWorkspaceSleepWake(false);
        QTest::qWait(50);
        QCOMPARE(calls, 0);
    }

    void invalidArguments() {
        QObject owner;
        QVERIFY(!ProxorSleepWake::Install(nullptr, [](bool) {}).installed);
        QVERIFY(!ProxorSleepWake::Install(&owner, std::function<void(bool)>()).installed);
    }
};

QTEST_GUILESS_MAIN(MacSleepWakeSmokeTest)
#include "mac_sleep_wake_smoke_test.moc"
