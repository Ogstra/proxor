#include "sys/wifi/WifiBackendIwd.hpp"

IwdWifiReader::IwdWifiReader(QDBusConnection bus, bool sandboxed, int budgetMs)
    : bus_(std::move(bus)), sandboxed_(sandboxed), budgetMs_(budgetMs) {}

ProxorWifi::WifiReading IwdWifiReader::read() { return readWithin(budgetMs_); }

ProxorWifi::WifiReading IwdWifiReader::readWithin(int) {
    return ProxorWifi::Unavailable(QString(), QStringLiteral("iwd"));
}

IwdPresence IwdWifiReader::presence() const { return presence_; }
