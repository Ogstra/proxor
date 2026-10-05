// src/platform/WakeCoordinator.hpp — what to do after the system woke from sleep (phase 56).
// Qt Core only, pure and deterministic: the caller injects the clock and the observed state and drives
// step() from its own timer. Tested on every runner.
#pragma once
#include <QString>
#include <QtGlobal>

namespace ProxorPlatform {

inline constexpr qint64 kWakeDedupeMs = 120000;            // a second wake signal inside this window is the same wake
inline constexpr qint64 kWakeNetworkWaitMaxMs = 60000;     // wait for reachability at most this long
inline constexpr qint64 kWakeNoReachabilityDelayMs = 2000; // no reachability backend: the old fixed 2 s delay
inline constexpr qint64 kWakeCoreGraceMs = 8000;           // CoreProcess's own restart (1 s + start) goes first
inline constexpr qint64 kWakeRunMaxMs = 180000;            // a run never lasts longer than this
inline constexpr int kWakePollMs = 1000;                   // re-evaluation cadence while waiting

enum class WakeSource { Native, TimerGap };
enum class WakeReachability { Unknown, Online, Offline };
struct WakeModes {
    bool systemProxy = false;
    bool tun = false;
}; // macOS only; always false elsewhere
struct WakeSnapshot {
    int profileId = -1;
    WakeModes modes;
}; // what ran when the system went to sleep
struct WakeObservation {
    qint64 nowMs = 0;
    WakeReachability reachability = WakeReachability::Unknown;
    bool blocked = false;       // startup Tun pending/failed, or exiting: the run ends without acting
    bool startPending = false;  // a profile start is in flight: never act during it
    bool coreRunning = false;   // dataStore->core_running
    int runningProfileId = -1;  // dataStore->started_id
};
enum class WakeAction { Wait, RestartProfile, CheckModes, UpdateSubscriptions, Finished };
enum class WakeNote { None, Blocked, NetworkTimedOut, RunTimedOut, CoreRestarting };
struct WakeStep {
    WakeAction action = WakeAction::Finished;
    int delayMs = -1;   // >= 0: call step() again after this delay; -1: the run is over
    int profileId = -1; // RestartProfile
    WakeModes modes;    // CheckModes: what was on before sleep
    WakeNote note = WakeNote::None;
};

class WakeCoordinator {
public:
    void noteSleep(qint64 nowMs, const WakeSnapshot &before);                    // cancels an active run, stores the snapshot
    bool noteWake(WakeSource source, qint64 nowMs, const WakeSnapshot &current); // false: duplicate / already running
    WakeStep step(const WakeObservation &o);
    bool active() const;
    void cancel();
    WakeSnapshot snapshot() const; // of the current or last run
    WakeSource source() const;     // of the current or last run

private:
    enum class Phase { Network, Core, Modes, Subs };
    bool active_ = false;
    Phase phase_ = Phase::Network;
    qint64 wakeAtMs_ = 0;
    qint64 lastWakeMs_ = -1;
    bool hasSleepSnapshot_ = false;
    WakeSnapshot sleepSnapshot_;
    WakeSnapshot snapshot_;
    qint64 coreDeadSinceMs_ = -1;
    bool networkReady_ = false;
    WakeSource source_ = WakeSource::Native;
};

// Which modes to re-apply after wake (macOS). Each true only if: on before sleep, profileRunning,
// the helper reports it off, and not parked (System Proxy) / paused (Tun) on purpose.
WakeModes DecideWakeModeRepair(const WakeModes &before, const WakeModes &helperReports, bool profileRunning,
                               bool systemProxyParked, bool tunPaused);

// Window-log lines (QCoreApplication::translate("Wake", ...)); every one starts with "Resumed from sleep: ".
QString WakeCoreRestartLine(const QString &profileName);    // "Resumed from sleep: the core had stopped; restarting %1."
QString WakeModesReappliedLine(const WakeModes &reapplied); // names "System Proxy", "Tun" or both
QString WakeModesCheckFailedLine(const QString &error);     // "Resumed from sleep: could not check System Proxy and Tun with the Proxor service: %1"

} // namespace ProxorPlatform
