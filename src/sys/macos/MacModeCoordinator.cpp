#include "MacModeCoordinator.h"

MacModeCoordinator::MacModeCoordinator(Hooks hooks, int graceMs, QObject *parent)
    : QObject(parent), hooks_(std::move(hooks)), graceMs_(graceMs) {
    timer_.setSingleShot(true);
    // A coarse timer may fire up to 5% early; the grace period must never be shortened.
    timer_.setTimerType(Qt::PreciseTimer);
    connect(&timer_, &QTimer::timeout, this, &MacModeCoordinator::onGraceTimeout);
}

void MacModeCoordinator::schedule() {
    waitingForStop_ = false;
    timer_.start(graceMs_);
}

void MacModeCoordinator::profileStopping(bool sem, bool prepareExit) {
    // Restart/switch (sem) and exit never pause: they stop and start the profile themselves.
    if (sem || prepareExit) return;
    stopInFlight_ = true;
    schedule();
}

void MacModeCoordinator::profileStopped() {
    stopInFlight_ = false;
    // The grace timer fired while the stop stage was still running: re-arm the full grace from
    // here (never pause directly) so a start right after a slow stop still cancels the pause.
    if (waitingForStop_) schedule();
}

void MacModeCoordinator::profileStarting() {
    timer_.stop();
    waitingForStop_ = false;
}

void MacModeCoordinator::profileStarted() {
    const bool sp = systemProxyParked_ && hooks_.systemProxyOn && hooks_.systemProxyOn();
    const bool tun = hooks_.tunOn && hooks_.tunOn() && hooks_.tunActive && !hooks_.tunActive();
    systemProxyParked_ = false;
    tunPaused_ = false;
    if ((sp || tun) && hooks_.resume) hooks_.resume(sp, tun);
}

void MacModeCoordinator::profileStartFailed() {
    if (hooks_.profileRunning && hooks_.profileRunning()) return;
    schedule();
}

void MacModeCoordinator::setSystemProxyParked(bool parked) {
    systemProxyParked_ = parked;
}

bool MacModeCoordinator::systemProxyParked() const {
    return systemProxyParked_;
}

bool MacModeCoordinator::tunPaused() const {
    return tunPaused_;
}

bool MacModeCoordinator::pauseScheduled() const {
    return timer_.isActive() || waitingForStop_;
}

bool MacModeCoordinator::stopInFlight() const {
    return stopInFlight_;
}

void MacModeCoordinator::reset() {
    timer_.stop();
    stopInFlight_ = false;
    waitingForStop_ = false;
    systemProxyParked_ = false;
    tunPaused_ = false;
}

void MacModeCoordinator::onGraceTimeout() {
    // The stop stage has not reported yet: wait for profileStopped() and re-arm the full grace.
    if (stopInFlight_) {
        waitingForStop_ = true;
        return;
    }
    // A start completed without cancelling, or the stop failed: nothing to pause.
    if (hooks_.profileRunning && hooks_.profileRunning()) return;

    // Evaluate the modes now; a proxy that is already parked is not restored twice.
    const bool sp = hooks_.systemProxyOn && hooks_.systemProxyOn() && !systemProxyParked_;
    const bool tun = hooks_.tunOn && hooks_.tunOn() && hooks_.tunActive && hooks_.tunActive();
    if (!sp && !tun) return;
    if (sp) systemProxyParked_ = true;
    if (tun) tunPaused_ = true;
    if (hooks_.pause) hooks_.pause(sp, tun);
}
