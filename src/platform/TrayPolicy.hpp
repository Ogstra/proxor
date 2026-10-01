#pragma once

// Pure decisions for hiding to the system tray (Qt Core only). With no tray host the window must stay
// reachable: minimize instead of hide, and show it instead of starting hidden.

#include "platform/PlatformCapabilities.hpp"

#include <QString>

namespace ProxorPlatform {

enum class CloseAction { HideToTray, Minimize };
CloseAction DecideCloseAction(const CapabilityStatus &tray);

enum class StartupVisibility { ShowWindow, StayHidden, WaitForTray };
StartupVisibility DecideStartupVisibility(bool startHidden, const CapabilityStatus &tray, int waitedMs, int maxWaitMs);

QString NoTrayCloseNotice(const CapabilityStatus &tray);
QString NoTrayStartupNotice(const CapabilityStatus &tray);

} // namespace ProxorPlatform
