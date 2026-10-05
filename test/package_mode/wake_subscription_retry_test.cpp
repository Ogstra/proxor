#include <QtTest>

#include "platform/WakeSubscriptionRetry.hpp"

using namespace ProxorPlatform;

namespace {

constexpr qint64 kAttemptRunMs = 500;

// Tiny fake world driven by a simulated clock: an attempt "runs" for kAttemptRunMs after StartUpdate, then ends
// and leaves the groups due (or not) depending on the scenario.
struct World {
    WakeSubscriptionRetry retry;
    qint64 now = 1000;
    bool blocked = false;
    bool scheduled = true;
    bool startPending = false;
    bool externalRunning = false;
    bool due = true;
    bool dueAfterAttempt = true;
    qint64 attemptEndsAt = -1;
    QList<qint64> startTimes;
    int startsWhileBusy = 0;
    SubscriptionRetryAction last = SubscriptionRetryAction::Wait;
    SubscriptionRetryStep lastStep;

    SubscriptionRetryStep tick() {
        if (attemptEndsAt >= 0 && now >= attemptEndsAt) {
            attemptEndsAt = -1;
            due = dueAfterAttempt;
        }
        SubscriptionRetryInputs in;
        in.nowMs = now;
        in.blocked = blocked;
        in.scheduled = scheduled;
        in.startPending = startPending;
        in.updateRunning = externalRunning || attemptEndsAt >= 0;
        in.anyDue = due;
        const bool busy = startPending || in.updateRunning;
        auto s = retry.step(in);
        if (s.action == SubscriptionRetryAction::StartUpdate) {
            if (busy) ++startsWhileBusy;
            startTimes << now;
            attemptEndsAt = now + kAttemptRunMs;
        }
        last = s.action;
        lastStep = s;
        return s;
    }

    // Runs until finished (delayMs < 0) or maxSteps.
    void run(int maxSteps = 100000) {
        for (int i = 0; i < maxSteps; ++i) {
            auto s = tick();
            if (s.delayMs < 0) return;
            now += s.delayMs;
        }
    }
};

} // namespace

class WakeSubscriptionRetryTest : public QObject {
    Q_OBJECT
private slots:
    void constants() {
        QCOMPARE(int(std::size(kWakeSubscriptionRetryDelaysMs)), 4);
        QCOMPARE(kWakeSubscriptionRetryDelaysMs[0], 3000);
        QCOMPARE(kWakeSubscriptionRetryDelaysMs[1], 10000);
        QCOMPARE(kWakeSubscriptionRetryDelaysMs[2], 30000);
        QCOMPARE(kWakeSubscriptionRetryDelaysMs[3], 60000);
        QCOMPARE(kWakeSubscriptionMaxAttempts, 5);
        QCOMPARE(kWakeSubscriptionPollMs, 1000);
        QCOMPARE(kWakeSubscriptionMaxMs, qint64(600000));
    }

    void nothingDueIsDone() {
        World w;
        w.due = false;
        w.retry.begin(w.now);
        auto s = w.tick();
        QCOMPARE(s.action, SubscriptionRetryAction::Done);
        QCOMPARE(s.delayMs, -1);
        QCOMPARE(w.retry.attempts(), 0);
        QVERIFY(!w.retry.active());
    }

    void firstAttemptImmediately() {
        World w;
        w.retry.begin(w.now);
        auto s = w.tick();
        QCOMPARE(s.action, SubscriptionRetryAction::StartUpdate);
        QCOMPARE(s.attempt, 1);
        QCOMPARE(s.delayMs, kWakeSubscriptionPollMs);
        QCOMPARE(w.startTimes.value(0), qint64(1000));
        QVERIFY(w.retry.active());
    }

    void backoffThenGiveUp() {
        World w;
        w.retry.begin(w.now);
        w.run();
        QCOMPARE(w.startTimes.size(), 5);
        // The end of an attempt is observed one poll after its start (attempt runs 500 ms, poll 1000 ms).
        const int delays[] = {3000, 10000, 30000, 60000};
        for (int i = 0; i < 4; ++i) {
            const qint64 observedEnd = w.startTimes[i] + kWakeSubscriptionPollMs;
            QCOMPARE(w.startTimes[i + 1], observedEnd + delays[i]);
        }
        QCOMPARE(w.last, SubscriptionRetryAction::GaveUp);
        QCOMPARE(w.lastStep.attempt, 5);
        QCOMPARE(w.lastStep.delayMs, -1);
        QCOMPARE(w.retry.attempts(), 5);
        QVERIFY(!w.retry.active());
        QCOMPARE(w.startsWhileBusy, 0);
    }

    void successEndsRightAfterAttempt() {
        World w;
        w.dueAfterAttempt = false;
        w.retry.begin(w.now);
        w.run();
        QCOMPARE(w.startTimes.size(), 1);
        QCOMPARE(w.last, SubscriptionRetryAction::Done);
        QCOMPARE(w.retry.attempts(), 1);
        QVERIFY(!w.retry.active());
    }

    void secondAttemptSucceeds() {
        World w;
        w.retry.begin(w.now);
        // First attempt fails, then the world heals before the second one finishes.
        for (int i = 0; i < 100000; ++i) {
            if (w.startTimes.size() == 2) w.dueAfterAttempt = false;
            auto s = w.tick();
            if (s.delayMs < 0) break;
            w.now += s.delayMs;
        }
        QCOMPARE(w.startTimes.size(), 2);
        QCOMPARE(w.last, SubscriptionRetryAction::Done);
        QCOMPARE(w.retry.attempts(), 2);
    }

    void foreignRunningUpdateIsNeverOverlapped() {
        World w;
        w.externalRunning = true;
        w.retry.begin(w.now);
        for (int i = 0; i < 20; ++i) {
            auto s = w.tick();
            QCOMPARE(s.action, SubscriptionRetryAction::Wait);
            QCOMPARE(s.delayMs, kWakeSubscriptionPollMs);
            w.now += s.delayMs;
        }
        QVERIFY(w.startTimes.isEmpty());
        // The other update finishes and clears the due state: nothing to do.
        w.externalRunning = false;
        w.due = false;
        auto s = w.tick();
        QCOMPARE(s.action, SubscriptionRetryAction::Done);
        QCOMPARE(w.retry.attempts(), 0);
    }

    void foreignRunningUpdateEndsStillDue() {
        World w;
        w.externalRunning = true;
        w.retry.begin(w.now);
        QCOMPARE(w.tick().action, SubscriptionRetryAction::Wait);
        w.now += 1000;
        w.externalRunning = false;
        QCOMPARE(w.tick().action, SubscriptionRetryAction::StartUpdate);
        QCOMPARE(w.startsWhileBusy, 0);
    }

    void pendingStartBlocksAttempts() {
        World w;
        w.startPending = true;
        w.retry.begin(w.now);
        for (int i = 0; i < 20; ++i) {
            auto s = w.tick();
            QCOMPARE(s.action, SubscriptionRetryAction::Wait);
            QCOMPARE(s.delayMs, kWakeSubscriptionPollMs);
            w.now += s.delayMs;
        }
        QVERIFY(w.startTimes.isEmpty());
        w.startPending = false;
        QCOMPARE(w.tick().action, SubscriptionRetryAction::StartUpdate);
        QCOMPARE(w.startsWhileBusy, 0);
    }

    void blockedOrUnscheduledIsDone() {
        {
            World w;
            w.blocked = true;
            w.retry.begin(w.now);
            QCOMPARE(w.tick().action, SubscriptionRetryAction::Done);
            QVERIFY(!w.retry.active());
        }
        {
            World w;
            w.scheduled = false;
            w.retry.begin(w.now);
            auto s = w.tick();
            QCOMPARE(s.action, SubscriptionRetryAction::Done);
            QCOMPARE(s.delayMs, -1);
            QVERIFY(!w.retry.active());
        }
    }

    void cancelStops() {
        World w;
        w.retry.begin(w.now);
        w.retry.cancel();
        QVERIFY(!w.retry.active());
        auto s = w.tick();
        QCOMPARE(s.action, SubscriptionRetryAction::Done);
        QCOMPARE(s.delayMs, -1);
        QVERIFY(w.startTimes.isEmpty());
    }

    void capGivesUpWhileStartStaysPending() {
        World w;
        w.startPending = true;
        w.retry.begin(w.now);
        const qint64 began = w.now;
        w.run(100000);
        QCOMPARE(w.last, SubscriptionRetryAction::GaveUp);
        QVERIFY(w.now - began >= kWakeSubscriptionMaxMs);
        QVERIFY(w.now - began < kWakeSubscriptionMaxMs + 2 * kWakeSubscriptionPollMs);
        QVERIFY(w.startTimes.isEmpty());
        QVERIFY(!w.retry.active());
    }

    void beginRestarts() {
        World w;
        w.retry.begin(w.now);
        w.run();
        QVERIFY(!w.retry.active());
        w.startTimes.clear();
        w.due = true;
        w.dueAfterAttempt = false;
        w.retry.begin(w.now);
        QVERIFY(w.retry.active());
        QCOMPARE(w.retry.attempts(), 0);
        QCOMPARE(w.tick().action, SubscriptionRetryAction::StartUpdate);
    }

    void gaveUpLine() {
        const QString line = WakeSubscriptionGaveUpLine(5);
        QVERIFY(line.startsWith("Resumed from sleep: "));
        QVERIFY(line.contains("5"));
    }
};

QTEST_GUILESS_MAIN(WakeSubscriptionRetryTest)
#include "wake_subscription_retry_test.moc"
