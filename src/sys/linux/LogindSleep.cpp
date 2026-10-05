#include "LogindSleep.hpp"
#include "sys/SleepWake.hpp"

namespace ProxorSleepWake {
InstallResult Install(QObject *, std::function<void(bool)>) { return {false, QStringLiteral("stub")}; }
} // namespace ProxorSleepWake

LogindSleepListener::LogindSleepListener(const QDBusConnection &bus, QObject *parent) : QObject(parent), bus_(bus) {}
bool LogindSleepListener::start() { return false; }
bool LogindSleepListener::serviceAvailable() const { return false; }
QString LogindSleepListener::detail() const { return detail_; }
void LogindSleepListener::onPrepareForSleep(bool) {}
