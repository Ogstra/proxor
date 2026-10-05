#include "WakeCoordinator.hpp"

#include <QCoreApplication>
#include <algorithm>

namespace ProxorPlatform {

namespace {
WakeStep waitStep(qint64 ms) {
    WakeStep s;
    s.action = WakeAction::Wait;
    s.delayMs = int(std::max<qint64>(ms, 0));
    return s;
}
} // namespace

void WakeCoordinator::noteSleep(qint64, const WakeSnapshot &before) {
    active_ = false;
    sleepSnapshot_ = before;
    hasSleepSnapshot_ = true;
}

bool WakeCoordinator::noteWake(WakeSource source, qint64 nowMs, const WakeSnapshot &current) {
    if (active_) return false;
    // Known limit: with the timer path only (e.g. Flatpak without logind) a second real sleep within
    // kWakeDedupeMs of the previous wake is not seen, because no sleep is noted in between.
    if (!hasSleepSnapshot_ && lastWakeMs_ >= 0 && nowMs - lastWakeMs_ < kWakeDedupeMs) return false;
    snapshot_ = hasSleepSnapshot_ ? sleepSnapshot_ : current;
    hasSleepSnapshot_ = false;
    lastWakeMs_ = wakeAtMs_ = nowMs;
    source_ = source;
    phase_ = Phase::Network;
    coreDeadSinceMs_ = -1;
    networkReady_ = false;
    active_ = true;
    return true;
}

WakeStep WakeCoordinator::step(const WakeObservation &o) {
    WakeStep done; // Finished, -1
    if (!active_) return done;
    if (o.blocked) {
        active_ = false;
        done.note = WakeNote::Blocked;
        return done;
    }
    if (o.nowMs - wakeAtMs_ >= kWakeRunMaxMs) {
        active_ = false;
        done.note = WakeNote::RunTimedOut;
        return done;
    }
    const qint64 elapsed = o.nowMs - wakeAtMs_;
    for (;;) {
        switch (phase_) {
        case Phase::Network:
            if (o.reachability == WakeReachability::Online) {
                networkReady_ = true;
                phase_ = Phase::Core;
            } else if (o.reachability == WakeReachability::Unknown) {
                if (elapsed < kWakeNoReachabilityDelayMs) return waitStep(kWakeNoReachabilityDelayMs - elapsed);
                networkReady_ = true;
                phase_ = Phase::Core;
            } else {
                if (elapsed < kWakeNetworkWaitMaxMs) return waitStep(kWakePollMs);
                networkReady_ = false;
                phase_ = Phase::Core;
            }
            break;
        case Phase::Core:
            if (snapshot_.profileId < 0) {
                phase_ = Phase::Modes;
            } else if (o.startPending) {
                return waitStep(kWakePollMs);
            } else if (o.coreRunning) {
                coreDeadSinceMs_ = -1;
                phase_ = Phase::Modes;
            } else {
                if (coreDeadSinceMs_ < 0) coreDeadSinceMs_ = o.nowMs;
                const qint64 remaining = kWakeCoreGraceMs - (o.nowMs - coreDeadSinceMs_);
                if (remaining > 0) return waitStep(std::min<qint64>(remaining, kWakePollMs));
                phase_ = Phase::Modes;
                WakeStep s;
                s.action = WakeAction::RestartProfile;
                s.delayMs = kWakePollMs;
                s.profileId = snapshot_.profileId;
                s.note = WakeNote::CoreRestarting;
                return s;
            }
            break;
        case Phase::Modes:
            if (!(snapshot_.modes.systemProxy || snapshot_.modes.tun)) {
                phase_ = Phase::Subs;
            } else if (o.startPending) {
                return waitStep(kWakePollMs);
            } else {
                phase_ = Phase::Subs;
                if (o.runningProfileId >= 0) {
                    WakeStep s;
                    s.action = WakeAction::CheckModes;
                    s.delayMs = 0;
                    s.modes = snapshot_.modes;
                    return s;
                }
            }
            break;
        case Phase::Subs:
            active_ = false;
            if (networkReady_) {
                WakeStep s;
                s.action = WakeAction::UpdateSubscriptions;
                s.delayMs = -1;
                return s;
            }
            done.note = WakeNote::NetworkTimedOut;
            return done;
        }
    }
}

bool WakeCoordinator::active() const { return active_; }
void WakeCoordinator::cancel() { active_ = false; }
WakeSnapshot WakeCoordinator::snapshot() const { return snapshot_; }
WakeSource WakeCoordinator::source() const { return source_; }

WakeModes DecideWakeModeRepair(const WakeModes &before, const WakeModes &helperReports, bool profileRunning,
                               bool systemProxyParked, bool tunPaused) {
    WakeModes r;
    if (!profileRunning) return r;
    r.systemProxy = before.systemProxy && !helperReports.systemProxy && !systemProxyParked;
    r.tun = before.tun && !helperReports.tun && !tunPaused;
    return r;
}

QString WakeCoreRestartLine(const QString &profileName) {
    return QCoreApplication::translate("Wake", "Resumed from sleep: the core had stopped; restarting %1.")
        .arg(profileName);
}

QString WakeModesReappliedLine(const WakeModes &reapplied) {
    QString what;
    if (reapplied.systemProxy && reapplied.tun)
        what = QCoreApplication::translate("Wake", "System Proxy and Tun");
    else if (reapplied.tun)
        what = QCoreApplication::translate("Wake", "Tun");
    else
        what = QCoreApplication::translate("Wake", "System Proxy");
    return QCoreApplication::translate("Wake", "Resumed from sleep: %1 was off in the Proxor service; turning it back on.")
        .arg(what);
}

QString WakeModesCheckFailedLine(const QString &error) {
    return QCoreApplication::translate(
               "Wake", "Resumed from sleep: could not check System Proxy and Tun with the Proxor service: %1")
        .arg(error);
}

} // namespace ProxorPlatform
