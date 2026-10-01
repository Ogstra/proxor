#pragma once
#include "platform/WifiSsid.hpp"
#include <functional>

class QObject;

namespace ProxorWifi {

// UI thread only. One implementation per OS (cmake-selected).
PermissionState CurrentWifiPermission();
// Calls done exactly once on context's thread (dropped if context is destroyed first).
void RequestWifiPermission(QObject *context, std::function<void(PermissionState)> done);
bool OpenWifiPermissionSettings();   // false when the platform has no settings page to open

} // namespace ProxorWifi
