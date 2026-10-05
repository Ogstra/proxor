#include "sys/linux/LogindSleep.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QSignalSpy>
#include <QtTest>

namespace {
const char kFakeConn[] = "fake-logind";
const char kOtherConn[] = "fake-impostor";

void EmitSleep(QDBusConnection &bus, bool value) {
    QDBusMessage m = QDBusMessage::createSignal(QString::fromLatin1(kLogindPath), QString::fromLatin1(kLogindManager),
                                                QStringLiteral("PrepareForSleep"));
    m << value;
    bus.send(m);
}
} // namespace

class LogindSleepTest : public QObject {
    Q_OBJECT

    QDBusConnection fake_{QStringLiteral("proxor-not-connected")};
    QDBusConnection other_{QStringLiteral("proxor-not-connected")};
    bool fakeOwnsName_ = false;

    bool registerFake() {
        fake_ = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString::fromLatin1(kFakeConn));
        fakeOwnsName_ = fake_.isConnected() && fake_.registerService(QString::fromLatin1(kLogindService));
        return fakeOwnsName_;
    }
    void connectOther() {
        other_ = QDBusConnection::connectToBus(QDBusConnection::SessionBus, QString::fromLatin1(kOtherConn));
    }

private slots:
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void cleanup() {
        if (fake_.isConnected()) {
            if (fakeOwnsName_) fake_.unregisterService(QString::fromLatin1(kLogindService));
        }
        fakeOwnsName_ = false;
        fake_ = QDBusConnection(QStringLiteral("proxor-not-connected"));
        other_ = QDBusConnection(QStringLiteral("proxor-not-connected"));
        QDBusConnection::disconnectFromBus(QString::fromLatin1(kFakeConn));
        QDBusConnection::disconnectFromBus(QString::fromLatin1(kOtherConn));
    }

    void orderTrueThenFalse() {
        QVERIFY(registerFake());
        LogindSleepListener l(QDBusConnection::sessionBus());
        QVERIFY(l.start());
        QVERIFY(l.serviceAvailable());
        QSignalSpy spy(&l, &LogindSleepListener::sleeping);
        EmitSleep(fake_, true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        EmitSleep(fake_, false);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 2, 3000);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(1).at(0).toBool(), false);
        QTest::qWait(200);
        QCOMPARE(spy.count(), 2);
    }

    void foreignSenderIgnored() {
        QVERIFY(registerFake());
        connectOther();
        QVERIFY(other_.isConnected());
        LogindSleepListener l(QDBusConnection::sessionBus());
        QVERIFY(l.start());
        QSignalSpy spy(&l, &LogindSleepListener::sleeping);
        EmitSleep(other_, true);
        QTest::qWait(500);
        QCOMPARE(spy.count(), 0);
        // the real owner still gets through
        EmitSleep(fake_, true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    }

    void absentThenAppears() {
        LogindSleepListener l(QDBusConnection::sessionBus());
        QVERIFY(l.start());
        QVERIFY(!l.serviceAvailable());
        QVERIFY(l.detail().contains(QStringLiteral("logind")));
        QSignalSpy spy(&l, &LogindSleepListener::sleeping);
        QVERIFY(registerFake());
        EmitSleep(fake_, true);
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void disconnectedBus() {
        LogindSleepListener l{QDBusConnection(QStringLiteral("proxor-not-connected"))};
        QVERIFY(!l.start());
        QVERIFY(!l.detail().isEmpty());
        QVERIFY(!l.serviceAvailable());
    }
};

QTEST_MAIN(LogindSleepTest)
#include "logind_sleep_test.moc"
