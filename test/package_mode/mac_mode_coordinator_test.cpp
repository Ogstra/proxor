#include "MacModeCoordinator.h"

#include <QtTest>

namespace {

// Generous margins so the timer-based cases hold on loaded CI runners.
const int G = 400;

using Call = QPair<bool, bool>;

struct Fake {
    bool sp = true;
    bool tun = true;
    bool active = true;
    bool running = false;
    QList<Call> pauses;
    QList<Call> resumes;

    MacModeCoordinator::Hooks hooks() {
        MacModeCoordinator::Hooks h;
        h.systemProxyOn = [this] { return sp; };
        h.tunOn = [this] { return tun; };
        h.tunActive = [this] { return active; };
        h.profileRunning = [this] { return running; };
        h.pause = [this](bool s, bool t) {
            pauses.append({s, t});
            if (t) active = false; // tun_stop
        };
        h.resume = [this](bool s, bool t) {
            resumes.append({s, t});
            if (t) active = true; // Tun started again
        };
        return h;
    }
};

} // namespace

class MacModeCoordinatorTest final : public QObject {
    Q_OBJECT

private slots:
    void userStopPausesAfterGrace();
    void logFlapScenario();
    void restartOrSwitchNeverPauses();
    void exitNeverPauses();
    void resumeAfterRealStop();
    void tunOnlyAndProxyOnly();
    void tunNotActiveNotPaused();
    void startFailedRearms();
    void doubleStopSinglePause();
    void parkedWhileStopped();
    void resetClears();
    void slowStopPausesAfterStopCompletes();
    void slowStopThenQuickStartNoFlap();
    void stopFailedNoPause();
    void runningAfterStoppedNoPause();
    void startDuringFlightCancels();
    void slowStopWaitsEvenWhenAlreadyNotRunning();
};

void MacModeCoordinatorTest::userStopPausesAfterGrace() {
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    QVERIFY(c.pauseScheduled());
    QTest::qWait(G / 2);
    QVERIFY(f.pauses.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
    QCOMPARE(f.pauses.first(), Call(true, true));
    QVERIFY(c.systemProxyParked());
    QVERIFY(c.tunPaused());
    QVERIFY(!c.pauseScheduled());
    QTest::qWait(G * 2);
    QCOMPARE(f.pauses.size(), 1);
    QVERIFY(f.resumes.isEmpty());
}

void MacModeCoordinatorTest::logFlapScenario() {
    // The owner's log: Stopping 18:39:18.909, Starting 18:39:18.940.
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    QTest::qWait(31);
    c.profileStarting();
    f.running = true;
    c.profileStarted();
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(f.resumes.isEmpty());
    QVERIFY(!c.systemProxyParked());
    QVERIFY(!c.tunPaused());
}

void MacModeCoordinatorTest::restartOrSwitchNeverPauses() {
    Fake f;
    f.running = true;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(true, false);
    c.profileStopped();
    f.running = false;
    QVERIFY(!c.pauseScheduled());
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
}

void MacModeCoordinatorTest::exitNeverPauses() {
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, true);
    c.profileStopped();
    QVERIFY(!c.pauseScheduled());
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
}

void MacModeCoordinatorTest::resumeAfterRealStop() {
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
    c.profileStarting();
    f.running = true;
    c.profileStarted();
    QCOMPARE(f.resumes.size(), 1);
    QCOMPARE(f.resumes.first(), Call(true, true));
    QVERIFY(!c.systemProxyParked());
    QVERIFY(!c.tunPaused());
    // Flags cleared: a second start does not resume again.
    c.profileStarted();
    QCOMPARE(f.resumes.size(), 1);
}

void MacModeCoordinatorTest::tunOnlyAndProxyOnly() {
    {
        Fake f;
        f.sp = false;
        MacModeCoordinator c(f.hooks(), G);
        c.profileStopping(false, false);
        c.profileStopped();
        QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
        QCOMPARE(f.pauses.first(), Call(false, true));
        c.profileStarting();
        f.running = true;
        c.profileStarted();
        QCOMPARE(f.resumes.size(), 1);
        QCOMPARE(f.resumes.first(), Call(false, true));
    }
    {
        Fake f;
        f.tun = false;
        f.active = false;
        MacModeCoordinator c(f.hooks(), G);
        c.profileStopping(false, false);
        c.profileStopped();
        QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
        QCOMPARE(f.pauses.first(), Call(true, false));
        c.profileStarting();
        f.running = true;
        c.profileStarted();
        QCOMPARE(f.resumes.size(), 1);
        QCOMPARE(f.resumes.first(), Call(true, false));
    }
    {
        Fake f;
        f.sp = false;
        f.tun = false;
        f.active = false;
        MacModeCoordinator c(f.hooks(), G);
        c.profileStopping(false, false);
        c.profileStopped();
        QTest::qWait(G * 3);
        c.profileStarting();
        f.running = true;
        c.profileStarted();
        QVERIFY(f.pauses.isEmpty());
        QVERIFY(f.resumes.isEmpty());
    }
}

void MacModeCoordinatorTest::tunNotActiveNotPaused() {
    Fake f;
    f.active = false; // Tun on but never started in the helper
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
    QCOMPARE(f.pauses.first(), Call(true, false));
    QVERIFY(!c.tunPaused());
    c.profileStarting();
    f.running = true;
    c.profileStarted();
    QCOMPARE(f.resumes.size(), 1);
    QCOMPARE(f.resumes.first(), Call(true, true));
}

void MacModeCoordinatorTest::startFailedRearms() {
    {
        Fake f;
        MacModeCoordinator c(f.hooks(), G);
        c.profileStopping(false, false);
        c.profileStopped();
        QTest::qWait(30);
        c.profileStarting(); // cancels the pause
        QTest::qWait(G * 2);
        QVERIFY(f.pauses.isEmpty());
        c.profileStartFailed(); // nothing runs
        QVERIFY(c.pauseScheduled());
        QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
    }
    {
        Fake f;
        MacModeCoordinator c(f.hooks(), G);
        c.profileStopping(false, false);
        c.profileStopped();
        c.profileStarting();
        f.running = true; // a profile is up anyway
        c.profileStartFailed();
        QTest::qWait(G * 3);
        QVERIFY(f.pauses.isEmpty());
    }
}

void MacModeCoordinatorTest::doubleStopSinglePause() {
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    c.profileStopping(false, false);
    c.profileStopped();
    QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
    QTest::qWait(G * 2);
    QCOMPARE(f.pauses.size(), 1);
}

void MacModeCoordinatorTest::parkedWhileStopped() {
    Fake f;
    f.tun = false;
    f.active = false;
    MacModeCoordinator c(f.hooks(), G);
    c.setSystemProxyParked(true);
    QVERIFY(c.systemProxyParked());
    f.running = true;
    c.profileStarted();
    QCOMPARE(f.resumes.size(), 1);
    QCOMPARE(f.resumes.first(), Call(true, false));
    QVERIFY(!c.systemProxyParked());
}

void MacModeCoordinatorTest::resetClears() {
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    c.reset();
    QVERIFY(!c.pauseScheduled());
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(!c.systemProxyParked());
    QVERIFY(!c.tunPaused());
    QVERIFY(!c.stopInFlight());

    c.setSystemProxyParked(true);
    c.reset();
    QVERIFY(!c.systemProxyParked());
}

void MacModeCoordinatorTest::slowStopPausesAfterStopCompletes() {
    Fake f;
    f.running = true; // the stop stage is still in flight
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    QVERIFY(c.stopInFlight());
    QTest::qWait(G * 2);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(c.pauseScheduled());
    QVERIFY(c.stopInFlight());

    f.running = false;
    c.profileStopped();
    QVERIFY(!c.stopInFlight());
    QTest::qWait(G / 2);
    QVERIFY(f.pauses.isEmpty()); // the full grace is re-armed
    QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
    QCOMPARE(f.pauses.first(), Call(true, true));
    QTest::qWait(G * 2);
    QCOMPARE(f.pauses.size(), 1);
}

void MacModeCoordinatorTest::slowStopThenQuickStartNoFlap() {
    Fake f;
    f.running = true;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    QTest::qWait(G * 2); // timer fires while the stop is in flight
    f.running = false;
    c.profileStopped();
    QTest::qWait(30);
    c.profileStarting();
    f.running = true;
    c.profileStarted();
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(f.resumes.isEmpty());
}

void MacModeCoordinatorTest::stopFailedNoPause() {
    Fake f;
    f.running = true;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    QTest::qWait(G * 2);
    c.profileStopped(); // stage failed: the profile is still running
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(!c.pauseScheduled());
}

void MacModeCoordinatorTest::runningAfterStoppedNoPause() {
    Fake f;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStopped();
    f.running = true; // a start through a path that never called profileStarting()
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
}

void MacModeCoordinatorTest::startDuringFlightCancels() {
    Fake f;
    f.running = true;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    c.profileStarting(); // a switch that won the race
    c.profileStopped();
    c.profileStarted();
    QTest::qWait(G * 3);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(f.resumes.isEmpty());
}

void MacModeCoordinatorTest::slowStopWaitsEvenWhenAlreadyNotRunning() {
    // started_id is already -1 but the stop stage has not reported yet: still no pause until
    // profileStopped() + a full grace period, so a start right after the stage still cancels it.
    Fake f;
    f.running = false;
    MacModeCoordinator c(f.hooks(), G);
    c.profileStopping(false, false);
    QTest::qWait(G * 2);
    QVERIFY(f.pauses.isEmpty());
    QVERIFY(c.pauseScheduled());
    c.profileStopped();
    QTest::qWait(G / 2);
    QVERIFY(f.pauses.isEmpty());
    QTRY_COMPARE_WITH_TIMEOUT(f.pauses.size(), 1, G * 6);
}

QTEST_GUILESS_MAIN(MacModeCoordinatorTest)
#include "mac_mode_coordinator_test.moc"
