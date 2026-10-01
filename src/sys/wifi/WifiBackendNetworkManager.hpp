#pragma once
#include "sys/wifi/WifiBackend.hpp"

#include <QDBusConnection>
#include <QString>

// Reads the connected Wi-Fi SSID from NetworkManager over D-Bus, falling back to nmcli when NetworkManager
// is not reachable outside a sandbox. QtCore + QtDBus only.
class NetworkManagerWifiReader final : public WifiBackend {
public:
    NetworkManagerWifiReader(QDBusConnection bus, QString nmcliProgram, bool sandboxed, int budgetMs = 3500);
    ProxorWifi::WifiReading read() override;

private:
    QDBusConnection bus_;
    QString nmcliProgram_;
    bool sandboxed_;
    int budgetMs_;
};
