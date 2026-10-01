#include "platform/TrayPolicy.hpp"

#include <QCoreApplication>

namespace ProxorPlatform {

CloseAction DecideCloseAction(const CapabilityStatus &tray) {
    return IsUsable(tray) ? CloseAction::HideToTray : CloseAction::Minimize;
}

StartupVisibility DecideStartupVisibility(bool startHidden, const CapabilityStatus &tray, int waitedMs, int maxWaitMs) {
    if (!startHidden) return StartupVisibility::ShowWindow;
    if (IsUsable(tray)) return StartupVisibility::StayHidden;
    return waitedMs < maxWaitMs ? StartupVisibility::WaitForTray : StartupVisibility::ShowWindow;
}

QString NoTrayCloseNotice(const CapabilityStatus &tray) {
    return QCoreApplication::translate("TrayPolicy",
                                       "No system tray is available, so Proxor was minimized instead of hidden and keeps running. %1 To quit, use Exit in the App menu.")
        .arg(tray.reason);
}

QString NoTrayStartupNotice(const CapabilityStatus &tray) {
    return QCoreApplication::translate("TrayPolicy", "Started with the window visible because no system tray is available. %1")
        .arg(tray.reason);
}

} // namespace ProxorPlatform
