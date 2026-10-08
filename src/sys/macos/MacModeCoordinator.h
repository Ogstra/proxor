#pragma once

// Grace-period pause/resume of System Proxy and Tun around a user profile stop.
// Qt Core only: no macOS APIs, so the behavior test compiles on every CI runner.
//
// A user stop (sem=false, not exit) schedules a pause after kMacPauseGraceMs; a start inside the
// grace period cancels it, so stop -> start in quick succession causes zero helper calls. If the
// stop stage outlives the grace period the coordinator waits for profileStopped() and re-arms the
// full grace period from there. With graceMs <= 0 (the user-stop setting) the pause happens at once,
// before the core stops; a failed stop (profile still running) resumes what was paused.

#include <QObject>
#include <QTimer>

#include <functional>

#include "MacHelperPolicy.h"

class MacModeCoordinator : public QObject {
    Q_OBJECT

public:
    struct Hooks {
        std::function<bool()> systemProxyOn;  // spmode_system_proxy
        std::function<bool()> tunOn;          // spmode_vpn
        std::function<bool()> tunActive;      // Tun requested/running in the helper (vpn_pid != 0)
        std::function<bool()> profileRunning; // started_id >= 0
        std::function<void(bool systemProxy, bool tun)> pause;  // restore proxy / tun_stop
        std::function<void(bool systemProxy, bool tun)> resume; // apply proxy / start Tun
    };

    explicit MacModeCoordinator(Hooks hooks, int graceMs = kMacPauseGraceMs, QObject *parent = nullptr);

    void profileStopping(bool sem, bool prepareExit); // only !sem && !prepareExit schedules a pause; marks a stop in flight
    void profileStopped();                            // stop stage finished (ok or failed); re-arms a deferred pause
    void profileStarting();                           // cancels a scheduled or deferred pause
    void profileStarted();                            // resumes what is paused/parked
    void profileStartFailed();                        // re-arms the pause if no profile runs
    void setSystemProxyParked(bool parked);           // toggle while stopped: apply on next start
    bool systemProxyParked() const;
    bool tunPaused() const;
    bool pauseScheduled() const;                      // timer running OR waiting for profileStopped()
    bool stopInFlight() const;
    void reset();                                     // exit / service lost: forget everything, stop the timer

private:
    void onGraceTimeout();
    void pauseNow(); // evaluate the modes and pause what is on, without waiting
    void schedule();

    Hooks hooks_;
    int graceMs_;
    QTimer timer_;
    bool stopInFlight_ = false;
    bool waitingForStop_ = false;
    bool systemProxyParked_ = false;
    bool tunPaused_ = false;
};
