#include "sys/wifi/WifiBackendNetworkManager.hpp"

#include <QDBusConnection>
#include <QFile>

std::unique_ptr<WifiBackend> CreatePlatformWifiBackend() {
    return std::make_unique<NetworkManagerWifiReader>(QDBusConnection::systemBus(), QStringLiteral("nmcli"),
                                                      QFile::exists(QStringLiteral("/.flatpak-info")));
}
