// macOS only (listed only in cmake/macos/macos.cmake). Observes sleep/wake; never sleeps or posts anything.
#include "sys/SleepWake.hpp"

namespace ProxorSleepWake {
InstallResult Install(QObject *, std::function<void(bool)>) {
    return {false, QStringLiteral("stub")};
}
}
