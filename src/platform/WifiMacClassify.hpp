#pragma once
#include "platform/WifiSsid.hpp"

#include <QString>

namespace ProxorWifi {

// Fields CoreWLAN exposes on macOS; the first four are readable without Location authorization.
struct MacWifiSnapshot {
    bool hasInterface = false;  // CWWiFiClient.sharedWiFiClient.interface != nil
    bool powerOn = false;       // [interface powerOn]
    QString ssid;               // [interface ssid] (empty when nil)
    bool associated = false;    // powerOn && rssiValue != 0 && wlanChannel != nil
};

// permissionPossible: true when the user can grant Location access (the name is then reported as a permission problem).
WifiReading ClassifyMacWifi(const MacWifiSnapshot &snapshot, bool permissionPossible, const QString &option3Reason = QString());

// rawStatus = CLAuthorizationStatus raw value; servicesEnabled = CLLocationManager.locationServicesEnabled
PermissionState MapMacLocationStatus(int rawStatus, bool servicesEnabled);

} // namespace ProxorWifi
