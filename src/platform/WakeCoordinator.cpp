// RED stub: wrong on purpose.
#include "WakeCoordinator.hpp"

namespace ProxorPlatform {
void WakeCoordinator::noteSleep(qint64, const WakeSnapshot &) {}
bool WakeCoordinator::noteWake(WakeSource, qint64, const WakeSnapshot &) { return true; }
WakeStep WakeCoordinator::step(const WakeObservation &) { return {}; }
bool WakeCoordinator::active() const { return false; }
void WakeCoordinator::cancel() {}
WakeSnapshot WakeCoordinator::snapshot() const { return {}; }
WakeSource WakeCoordinator::source() const { return WakeSource::Native; }
WakeModes DecideWakeModeRepair(const WakeModes &, const WakeModes &, bool, bool, bool) { return {}; }
QString WakeCoreRestartLine(const QString &) { return {}; }
QString WakeModesReappliedLine(const WakeModes &) { return {}; }
QString WakeModesCheckFailedLine(const QString &) { return {}; }
} // namespace ProxorPlatform
