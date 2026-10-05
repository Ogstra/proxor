#include "platform/WakeCoordinator.hpp"

#include <QList>
#include <QtTest>
#include <algorithm>
#include <functional>

using namespace ProxorPlatform;

namespace {
struct Run {
    QList<WakeStep> steps; // non-Wait steps
    qint64 endMs = 0;
    bool over = false;
};

Run drive(WakeCoordinator &c, qint64 startMs, const std::function<WakeObservation(qint64)> &world, qint64 limitMs) {
    Run r;
    qint64 now = startMs;
    while (now <= limitMs) {
        WakeStep s = c.step(world(now));
        if (s.action != WakeAction::Wait) r.steps.append(s);
        if (s.delayMs < 0) {
            r.over = true;
            break;
        }
        now += std::max(s.delayMs, 0);
    }
    r.endMs = now;
    return r;
}

int count(const Run &r, WakeAction a) {
    int n = 0;
    for (const auto &s : r.steps) n += s.action == a;
    return n;
}

WakeObservation online(qint64 now) {
    WakeObservation o;
    o.nowMs = now;
    o.reachability = WakeReachability::Online;
    return o;
}

WakeSnapshot prof(int id, bool sp = false, bool tun = false) {
    WakeSnapshot s;
    s.profileId = id;
    s.modes.systemProxy = sp;
    s.modes.tun = tun;
    return s;
}
} // namespace

class WakeCoordinatorTest : public QObject {
    Q_OBJECT
private slots:
    void constants() {
        QCOMPARE(kWakeDedupeMs, qint64(120000));
        QCOMPARE(kWakeNetworkWaitMaxMs, qint64(60000));
        QCOMPARE(kWakeNoReachabilityDelayMs, qint64(2000));
        QCOMPARE(kWakeCoreGraceMs, qint64(8000));
        QCOMPARE(kWakeRunMaxMs, qint64(180000));
        QCOMPARE(kWakePollMs, 1000);
    }

    void dedupe() {
        const qint64 t = 1000000;
        {
            WakeCoordinator c;
            QVERIFY(c.noteWake(WakeSource::Native, t, prof(-1)));
            QVERIFY(!c.noteWake(WakeSource::TimerGap, t + 30000, prof(-1))); // active
            drive(c, t, online, t + 10000);
            QVERIFY(!c.active());
            QVERIFY(!c.noteWake(WakeSource::TimerGap, t + 30000, prof(-1))); // inside window
        }
        {
            WakeCoordinator c;
            QVERIFY(c.noteWake(WakeSource::TimerGap, t, prof(-1)));
            drive(c, t, online, t + 10000);
            QVERIFY(!c.noteWake(WakeSource::Native, t + 30000, prof(-1)));
        }
        {
            WakeCoordinator c;
            QVERIFY(c.noteWake(WakeSource::Native, t, prof(-1)));
            drive(c, t, online, t + 10000);
            QVERIFY(c.noteWake(WakeSource::TimerGap, t + 121000, prof(-1)));
        }
        {
            WakeCoordinator c;
            QVERIFY(c.noteWake(WakeSource::Native, t, prof(-1)));
            drive(c, t, online, t + 10000);
            c.noteSleep(t + 10000, prof(-1));
            QVERIFY(c.noteWake(WakeSource::Native, t + 20000, prof(-1)));
            QVERIFY(c.active());
            QVERIFY(!c.noteWake(WakeSource::TimerGap, t + 21000, prof(-1)));
        }
    }

    void snapshotMemory() {
        const qint64 t = 5000;
        WakeCoordinator c;
        c.noteSleep(t, prof(7, true));
        QVERIFY(c.noteWake(WakeSource::Native, t + 100, prof(-1)));
        QCOMPARE(c.snapshot().profileId, 7);
        QVERIFY(c.snapshot().modes.systemProxy);
        QCOMPARE(c.source(), WakeSource::Native);
        drive(c, t + 100, online, t + 20000);
        // consumed: next timer wake uses current
        QVERIFY(c.noteWake(WakeSource::TimerGap, t + 130000, prof(3)));
        QCOMPARE(c.snapshot().profileId, 3);
        QCOMPARE(c.source(), WakeSource::TimerGap);

        WakeCoordinator d;
        QVERIFY(d.noteWake(WakeSource::TimerGap, t, prof(4)));
        QCOMPARE(d.snapshot().profileId, 4);
        QVERIFY(d.active());
        d.noteSleep(t + 10, prof(4));
        QVERIFY(!d.active());
    }

    void reachability() {
        const qint64 t = 0;
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(-1));
            WakeStep s = c.step(online(t));
            QCOMPARE(s.action, WakeAction::UpdateSubscriptions);
            QCOMPARE(s.delayMs, -1);
            QVERIFY(!c.active());
        }
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(-1));
            auto unknown = [](qint64 now) {
                WakeObservation o;
                o.nowMs = now;
                o.reachability = WakeReachability::Unknown;
                return o;
            };
            QCOMPARE(c.step(unknown(t)).action, WakeAction::Wait);
            QCOMPARE(c.step(unknown(t + 1999)).action, WakeAction::Wait);
            QCOMPARE(c.step(unknown(t + 2000)).action, WakeAction::UpdateSubscriptions);
        }
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(-1));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o;
                o.nowMs = now;
                o.reachability = now >= 30000 ? WakeReachability::Online : WakeReachability::Offline;
                return o;
            }, 100000);
            QVERIFY(r.over);
            QCOMPARE(r.endMs, qint64(30000));
            QCOMPARE(r.steps.last().action, WakeAction::UpdateSubscriptions);
        }
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7, true));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o;
                o.nowMs = now;
                o.reachability = WakeReachability::Offline;
                o.coreRunning = true;
                o.runningProfileId = 7;
                return o;
            }, 200000);
            QVERIFY(r.over);
            QCOMPARE(count(r, WakeAction::UpdateSubscriptions), 0);
            QCOMPARE(count(r, WakeAction::CheckModes), 1);
            QCOMPARE(r.steps.last().action, WakeAction::Finished);
            QCOMPARE(r.steps.last().note, WakeNote::NetworkTimedOut);
            QVERIFY(!c.active());
        }
    }

    void coreRestart() {
        const qint64 t = 0;
        auto dead = [](qint64 now) {
            WakeObservation o = online(now);
            o.coreRunning = false;
            o.runningProfileId = -1;
            return o;
        };
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            Run r = drive(c, t, dead, 100000);
            QCOMPARE(count(r, WakeAction::RestartProfile), 1);
            const WakeStep &s = r.steps.first();
            QCOMPARE(s.action, WakeAction::RestartProfile);
            QCOMPARE(s.profileId, 7);
            QCOMPARE(s.note, WakeNote::CoreRestarting);
            QVERIFY(r.over);
        }
        {   // first dead observation at t+0; restart not before 8000 ms later
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            QCOMPARE(c.step(dead(t)).action, WakeAction::Wait);
            QCOMPARE(c.step(dead(t + 7999)).action, WakeAction::Wait);
            WakeStep s = c.step(dead(t + 8000));
            QCOMPARE(s.action, WakeAction::RestartProfile);
            QVERIFY(s.delayMs >= 0);
        }
        {   // core back at +5 s
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o = online(now);
                o.coreRunning = now >= 5000;
                o.runningProfileId = now >= 5000 ? 7 : -1;
                return o;
            }, 100000);
            QCOMPARE(count(r, WakeAction::RestartProfile), 0);
            QVERIFY(r.over);
        }
        {   // no profile before sleep
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(-1));
            Run r = drive(c, t, dead, 100000);
            QCOMPARE(count(r, WakeAction::RestartProfile), 0);
        }
        {   // core running, nothing started
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o = online(now);
                o.coreRunning = true;
                o.runningProfileId = -1;
                return o;
            }, 100000);
            QCOMPARE(count(r, WakeAction::RestartProfile), 0);
        }
        {   // start pending
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o = online(now);
                o.startPending = true;
                return o;
            }, 20000);
            QCOMPARE(count(r, WakeAction::RestartProfile), 0);
        }
    }

    void modeCheck() {
        const qint64 t = 0;
        auto running = [](qint64 now) {
            WakeObservation o = online(now);
            o.coreRunning = true;
            o.runningProfileId = 7;
            return o;
        };
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7, true, false));
            Run r = drive(c, t, running, 100000);
            QCOMPARE(count(r, WakeAction::CheckModes), 1);
            for (const auto &s : r.steps)
                if (s.action == WakeAction::CheckModes) {
                    QVERIFY(s.modes.systemProxy);
                    QVERIFY(!s.modes.tun);
                }
            QCOMPARE(r.steps.last().action, WakeAction::UpdateSubscriptions);
        }
        {   // no running profile
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7, true, true));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o = online(now);
                o.coreRunning = true;
                o.runningProfileId = -1;
                return o;
            }, 100000);
            QCOMPARE(count(r, WakeAction::CheckModes), 0);
        }
        {   // modes all off
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            Run r = drive(c, t, running, 100000);
            QCOMPARE(count(r, WakeAction::CheckModes), 0);
        }
    }

    void blockedAndTimeout() {
        const qint64 t = 0;
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7, true));
            WakeObservation o = online(t);
            o.blocked = true;
            WakeStep s = c.step(o);
            QCOMPARE(s.action, WakeAction::Finished);
            QCOMPARE(s.note, WakeNote::Blocked);
            QCOMPARE(s.delayMs, -1);
            QVERIFY(!c.active());
        }
        {
            WakeCoordinator c;
            c.noteWake(WakeSource::Native, t, prof(7));
            Run r = drive(c, t, [](qint64 now) {
                WakeObservation o = online(now);
                o.startPending = true;
                return o;
            }, 300000);
            QVERIFY(r.over);
            QCOMPARE(r.steps.last().action, WakeAction::Finished);
            QCOMPARE(r.steps.last().note, WakeNote::RunTimedOut);
            QVERIFY(r.endMs >= kWakeRunMaxMs);
            QVERIFY(r.endMs < kWakeRunMaxMs + 2000);
        }
    }

    void cancelStopsRun() {
        WakeCoordinator c;
        c.noteWake(WakeSource::Native, 0, prof(7));
        QVERIFY(c.active());
        c.cancel();
        QVERIFY(!c.active());
        QCOMPARE(c.step(online(10)).delayMs, -1);
    }

    void modeRepair() {
        WakeModes none, sp, tun, both;
        sp.systemProxy = true;
        tun.tun = true;
        both.systemProxy = both.tun = true;
        auto eq = [](const WakeModes &m, bool s, bool t) { return m.systemProxy == s && m.tun == t; };
        QVERIFY(eq(DecideWakeModeRepair(both, none, false, false, false), false, false));
        QVERIFY(eq(DecideWakeModeRepair(sp, none, true, false, false), true, false));
        QVERIFY(eq(DecideWakeModeRepair(sp, none, true, true, false), false, false));
        QVERIFY(eq(DecideWakeModeRepair(tun, none, true, false, false), false, true));
        QVERIFY(eq(DecideWakeModeRepair(tun, none, true, false, true), false, false));
        QVERIFY(eq(DecideWakeModeRepair(both, both, true, false, false), false, false));
        QVERIFY(eq(DecideWakeModeRepair(none, none, true, false, false), false, false));
        QVERIFY(eq(DecideWakeModeRepair(both, sp, true, false, false), false, true));
        QVERIFY(eq(DecideWakeModeRepair(both, none, true, true, true), false, false));
    }

    void logLines() {
        const QString p = "Resumed from sleep: ";
        QVERIFY(WakeCoreRestartLine("MyProfile").startsWith(p));
        QVERIFY(WakeCoreRestartLine("MyProfile").contains("MyProfile"));
        WakeModes both, sp, tun;
        both.systemProxy = both.tun = true;
        sp.systemProxy = true;
        tun.tun = true;
        const QString b = WakeModesReappliedLine(both);
        QVERIFY(b.startsWith(p));
        QVERIFY(b.contains("System Proxy"));
        QVERIFY(b.contains("Tun"));
        QVERIFY(WakeModesReappliedLine(sp).contains("System Proxy"));
        QVERIFY(!WakeModesReappliedLine(sp).contains("Tun"));
        QVERIFY(WakeModesReappliedLine(tun).contains("Tun"));
        QVERIFY(!WakeModesReappliedLine(tun).contains("System Proxy"));
        QVERIFY(WakeModesCheckFailedLine("boom").startsWith(p));
        QVERIFY(WakeModesCheckFailedLine("boom").contains("boom"));
    }
};

QTEST_GUILESS_MAIN(WakeCoordinatorTest)
#include "wake_coordinator_test.moc"
