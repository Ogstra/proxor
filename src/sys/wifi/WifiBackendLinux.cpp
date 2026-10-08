#include "sys/wifi/WifiBackendChain.hpp"

#include <QDBusConnection>
#include <QFile>

// NetworkManager first (D-Bus, then nmcli); iwd when NetworkManager does not manage a Wi-Fi adapter.
std::unique_ptr<WifiBackend> CreatePlatformWifiBackend() {
    const bool sandboxed = QFile::exists(QStringLiteral("/.flatpak-info"));
    return std::make_unique<NmThenIwdWifiReader>(
        std::make_unique<NetworkManagerWifiReader>(QDBusConnection::systemBus(), QStringLiteral("nmcli"), sandboxed),
        std::make_unique<IwdWifiReader>(QDBusConnection::systemBus(), sandboxed));
}
