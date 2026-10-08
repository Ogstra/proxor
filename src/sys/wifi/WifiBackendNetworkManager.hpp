#pragma once
#include "sys/wifi/WifiBackend.hpp"

#include <QDBusConnection>
#include <QString>

// Reads the connected Wi-Fi SSID from NetworkManager over D-Bus, falling back to nmcli when NetworkManager
// is not reachable outside a sandbox. QtCore + QtDBus only.
// How NetworkManager answered the last read (the chain asks iwd only when it does not manage Wi-Fi).
enum class NmWifiPresence { ManagesWifi, NoWifiDevice, NotManagingWifi, Unreachable, TimedOut, Failed };

class NetworkManagerWifiReader final : public WifiBackend {
public:
    NetworkManagerWifiReader(QDBusConnection bus, QString nmcliProgram, bool sandboxed, int budgetMs = 3500);
    ProxorWifi::WifiReading read() override;
    NmWifiPresence presence() const;  // how the last read() went

private:
    NmWifiPresence presence_ = NmWifiPresence::Unreachable;
    QDBusConnection bus_;
    QString nmcliProgram_;
    bool sandboxed_;
    int budgetMs_;
};
