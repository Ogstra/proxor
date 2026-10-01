#include "sys/wifi/WifiPermission.hpp"

#include <QMetaObject>
#include <QObject>

ProxorWifi::PermissionState ProxorWifi::CurrentWifiPermission() {
    return PermissionState::NotRequired;
}

void ProxorWifi::RequestWifiPermission(QObject *context, std::function<void(PermissionState)> done) {
    if (!context || !done) return;
    QMetaObject::invokeMethod(context, [done] { done(ProxorWifi::PermissionState::NotRequired); }, Qt::QueuedConnection);
}

bool ProxorWifi::OpenWifiPermissionSettings() {
    return false;
}
