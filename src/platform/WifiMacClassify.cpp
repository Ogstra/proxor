#include "platform/WifiMacClassify.hpp"

#include <QCoreApplication>

namespace ProxorWifi {

namespace {
    QString T(const char *text) { return QCoreApplication::translate("WifiSsid", text); }
    const char *const kSource = "CoreWLAN";
} // namespace

WifiReading ClassifyMacWifi(const MacWifiSnapshot &snapshot, bool permissionPossible, const QString &option3Reason) {
    const QString source = QString::fromLatin1(kSource);
    if (!snapshot.hasInterface) return Unavailable(T("This Mac has no Wi-Fi interface."), source);
    if (!snapshot.powerOn) return NotConnected(T("Wi-Fi is turned off."), source);
    if (!snapshot.ssid.isEmpty()) return Connected(snapshot.ssid, source);
    if (snapshot.associated) {
        if (permissionPossible) {
            return PermissionNeeded(T("macOS hides the Wi-Fi network name until Proxor is allowed to use Location "
                                      "Services. Proxor does not use your location."),
                                    source);
        }
        return Unavailable(option3Reason, source);
    }
    return NotConnected(QString(), source);
}

PermissionState MapMacLocationStatus(int rawStatus, bool servicesEnabled) {
    if (!servicesEnabled) return PermissionState::ServicesDisabled;
    switch (rawStatus) {
        case 0: return PermissionState::NotDetermined;
        case 1: return PermissionState::Restricted;
        case 2: return PermissionState::Denied;
        case 3:
        case 4: return PermissionState::Granted;
        default: return PermissionState::NotDetermined;
    }
}

} // namespace ProxorWifi
