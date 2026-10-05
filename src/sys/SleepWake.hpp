#pragma once
// OS-neutral native sleep/wake facade (phase 56). macOS: src/sys/macos/MacSleepWake.mm (NSWorkspace
// WillSleep/DidWake). Linux: src/sys/linux/LogindSleep.cpp (systemd-logind PrepareForSleep, system bus).
// Windows neither links nor calls it: Windows keeps the subscription-timer heuristic only.
#include <QString>
#include <functional>
class QObject;
namespace ProxorSleepWake {
struct InstallResult {
    bool installed = false; // native source subscribed (events may still never arrive, e.g. no logind)
    QString detail;         // one line for the log file: source name, or why it is unavailable
};
// Calls onEvent(true) right before the system sleeps and onEvent(false) after it woke, on owner's thread;
// synchronously when the native notification already arrives on that thread, so the pre-sleep snapshot
// is taken before the system sleeps. Stops when owner is destroyed. Call once per owner.
InstallResult Install(QObject *owner, std::function<void(bool sleeping)> onEvent);
}
