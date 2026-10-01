#include "sys/WifiMonitor.hpp"

#include <QElapsedTimer>
#include <QMutex>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>
#include <atomic>

using namespace ProxorWifi;

namespace {

struct FakeState {
    QMutex mutex;
    QList<WifiReading> script;  // read i returns script[min(i, size-1)]
    std::atomic<int> reads{0};
    std::atomic<int> running{0};
    std::atomic<int> maxRunning{0};
    std::atomic<int> sleepMs{0};
    Qt::HANDLE lastThread = nullptr;
};

class FakeBackend : public WifiBackend {
public:
    explicit FakeBackend(std::shared_ptr<FakeState> s) : s_(std::move(s)) {}
    WifiReading read() override {
        const int now = ++s_->running;
        int prev = s_->maxRunning.load();
        while (now > prev && !s_->maxRunning.compare_exchange_weak(prev, now)) {}
        const int index = s_->reads.fetch_add(1);
        if (s_->sleepMs > 0) QThread::msleep(s_->sleepMs);
        WifiReading r;
        {
            QMutexLocker lock(&s_->mutex);
            s_->lastThread = QThread::currentThreadId();
            r = s_->script.isEmpty() ? NotConnected("", "test") : s_->script.at(qMin(index, int(s_->script.size()) - 1));
        }
        --s_->running;
        return r;
    }

private:
    std::shared_ptr<FakeState> s_;
};

} // namespace

class WifiMonitorTest : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() { qRegisterMetaType<ProxorWifi::WifiReading>(); }

    void activationDoesNotBlock() {
        auto s = std::make_shared<FakeState>();
        s->sleepMs = 400;
        s->script = {Connected("A", "test")};
        WifiMonitor m(std::make_unique<FakeBackend>(s), 50);
        QSignalSpy spy(&m, &WifiMonitor::readingChanged);
        QElapsedTimer t;
        t.start();
        m.setActive(true);
        QVERIFY2(t.elapsed() < 100, "setActive blocked on the backend");
        QVERIFY(!m.hasReading());
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
        QVERIFY(m.hasReading());
        QVERIFY(s->lastThread != QThread::currentThreadId());
        QCOMPARE(m.currentSsid(), QString("A"));
    }

    void inactiveDoesNotRead() {
        auto s = std::make_shared<FakeState>();
        WifiMonitor m(std::make_unique<FakeBackend>(s), 30);
        QTest::qWait(150);
        QCOMPARE(s->reads.load(), 0);
        QVERIFY(!m.isActive());
        m.setActive(true);
        QTRY_VERIFY_WITH_TIMEOUT(s->reads.load() >= 2, 3000);
        m.setActive(false);
        QTest::qWait(100);  // let an in-flight read finish
        const int after = s->reads.load();
        QTest::qWait(200);
        QCOMPARE(s->reads.load(), after);
    }

    void refreshNowOnInactive() {
        auto s = std::make_shared<FakeState>();
        WifiMonitor m(std::make_unique<FakeBackend>(s), 30);
        m.refreshNow();
        QTRY_VERIFY_WITH_TIMEOUT(m.hasReading(), 3000);
        QTest::qWait(150);
        QCOMPARE(s->reads.load(), 1);
    }

    void refreshNowCoalesces() {
        auto s = std::make_shared<FakeState>();
        s->sleepMs = 200;
        WifiMonitor m(std::make_unique<FakeBackend>(s), 5000);
        m.refreshNow();
        m.refreshNow();
        m.refreshNow();
        QTRY_VERIFY_WITH_TIMEOUT(s->reads.load() >= 2, 3000);
        QTest::qWait(500);
        QCOMPARE(s->reads.load(), 2);
        QCOMPARE(s->maxRunning.load(), 1);
    }

    void ssidChangedSequence() {
        auto s = std::make_shared<FakeState>();
        s->script = {NotConnected("", "test"), Connected("A", "test"), Connected("A", "test"),
                     Unavailable("x", "test"), Connected("B", "test"), NotConnected("", "test")};
        WifiMonitor m(std::make_unique<FakeBackend>(s), 20);
        QStringList seen, cachedInSlot;
        connect(&m, &WifiMonitor::ssidChanged, this, [&](const QString &ssid) {
            seen << ssid;
            cachedInSlot << WifiMonitor::cachedSsid();
        });
        m.setActive(true);
        QTRY_COMPARE_WITH_TIMEOUT(seen.size(), 3, 5000);
        m.setActive(false);
        QCOMPARE(seen, (QStringList{"A", "B", ""}));
        QCOMPARE(cachedInSlot, seen);
    }

    void failuresNeverChangeSsid() {
        auto s = std::make_shared<FakeState>();
        s->script = {Connected("A", "test"), Unavailable("x", "test"), PermissionNeeded("y", "test")};
        WifiMonitor m(std::make_unique<FakeBackend>(s), 20);
        QStringList ssids;
        int readings = 0;
        connect(&m, &WifiMonitor::ssidChanged, this, [&](const QString &v) { ssids << v; });
        connect(&m, &WifiMonitor::readingChanged, this, [&](const WifiReading &) { ++readings; });
        m.setActive(true);
        QTRY_COMPARE_WITH_TIMEOUT(readings, 3, 5000);
        m.setActive(false);
        QCOMPARE(ssids, (QStringList{"A"}));
        QCOMPARE(m.currentSsid(), QString("A"));
        QCOMPARE(WifiMonitor::cachedSsid(), QString("A"));
        QCOMPARE(m.lastReading().state, ReadState::PermissionNeeded);
    }

    void destroyDuringRead() {
        auto s = std::make_shared<FakeState>();
        s->sleepMs = 400;
        s->script = {Connected("Z", "test")};
        int signals_ = 0;
        QElapsedTimer t;
        {
            WifiMonitor m(std::make_unique<FakeBackend>(s), 50);
            connect(&m, &WifiMonitor::readingChanged, this, [&](const WifiReading &) { ++signals_; });
            m.setActive(true);
            QTest::qWait(100);
            t.start();
        }
        QVERIFY(t.elapsed() < 5000);
        QTest::qWait(600);
        QCOMPARE(signals_, 0);
    }

    void appInstance() {
        QVERIFY(WifiMonitor::appInstance() == nullptr);
        auto s = std::make_shared<FakeState>();
        {
            WifiMonitor m(std::make_unique<FakeBackend>(s), 50);
            WifiMonitor::setAppInstance(&m);
            QVERIFY(WifiMonitor::appInstance() == &m);
        }
        QVERIFY(WifiMonitor::appInstance() == nullptr);
    }

    void hasReadingWithDefaultReading() {
        auto s = std::make_shared<FakeState>();
        s->script = {WifiReading{}};  // equals the default-constructed (Unavailable, empty)
        WifiMonitor m(std::make_unique<FakeBackend>(s), 5000);
        QVERIFY(!m.hasReading());
        m.refreshNow();
        QTRY_VERIFY_WITH_TIMEOUT(m.hasReading(), 3000);
    }
};

QTEST_GUILESS_MAIN(WifiMonitorTest)
#include "wifi_monitor_test.moc"
