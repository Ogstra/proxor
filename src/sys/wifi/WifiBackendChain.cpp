#include "sys/wifi/WifiBackendChain.hpp"

using namespace ProxorWifi;

WifiReading ChooseWifiReading(const WifiReading &nm, NmWifiPresence, const IwdRead &) { return nm; }

NmThenIwdWifiReader::NmThenIwdWifiReader(std::unique_ptr<NetworkManagerWifiReader> nm, std::unique_ptr<IwdWifiReader> iwd, int totalBudgetMs)
    : nm_(std::move(nm)), iwd_(std::move(iwd)), totalBudgetMs_(totalBudgetMs) {}

WifiReading NmThenIwdWifiReader::read() { return nm_->read(); }
