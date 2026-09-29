#include "MacModeCoordinator.h"

// RED stubs (50-16 task 1)
MacModeCoordinator::MacModeCoordinator(Hooks hooks, int graceMs, QObject *parent)
    : QObject(parent), hooks_(std::move(hooks)), graceMs_(graceMs) {}

void MacModeCoordinator::profileStopping(bool, bool) {}
void MacModeCoordinator::profileStopped() {}
void MacModeCoordinator::profileStarting() {}
void MacModeCoordinator::profileStarted() {}
void MacModeCoordinator::profileStartFailed() {}
void MacModeCoordinator::setSystemProxyParked(bool) {}
bool MacModeCoordinator::systemProxyParked() const { return false; }
bool MacModeCoordinator::tunPaused() const { return false; }
bool MacModeCoordinator::pauseScheduled() const { return false; }
bool MacModeCoordinator::stopInFlight() const { return false; }
void MacModeCoordinator::reset() {}
void MacModeCoordinator::onGraceTimeout() {}
void MacModeCoordinator::schedule() {}
