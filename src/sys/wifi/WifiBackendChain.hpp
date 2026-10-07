#pragma once
#include "sys/wifi/WifiBackendIwd.hpp"
#include "sys/wifi/WifiBackendNetworkManager.hpp"
#include <functional>
#include <memory>
#include <utility>

using IwdRead = std::function<std::pair<ProxorWifi::WifiReading, IwdPresence>()>;

// Pure choice (no bus): NetworkManager first; iwd only when NetworkManager is not running, has no Wi-Fi device
// or manages none, and only an iwd that really answered (Present) replaces the NetworkManager reading.
// readIwd is called at most once and only when needed.
ProxorWifi::WifiReading ChooseWifiReading(const ProxorWifi::WifiReading &nm, NmWifiPresence nmPresence, const IwdRead &readIwd);

class NmThenIwdWifiReader final : public WifiBackend {
public:
    NmThenIwdWifiReader(std::unique_ptr<NetworkManagerWifiReader> nm, std::unique_ptr<IwdWifiReader> iwd, int totalBudgetMs = 3800);
    ProxorWifi::WifiReading read() override;
private:
    std::unique_ptr<NetworkManagerWifiReader> nm_;
    std::unique_ptr<IwdWifiReader> iwd_;
    int totalBudgetMs_;
};
