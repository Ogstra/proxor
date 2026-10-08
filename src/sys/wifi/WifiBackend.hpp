#pragma once
#include "platform/WifiSsid.hpp"
#include <memory>

class WifiBackend {
public:
    virtual ~WifiBackend() = default;
    // Runs on the WifiMonitor worker thread. Must return within ~4 s (bound any process/IPC wait).
    virtual ProxorWifi::WifiReading read() = 0;
};

// Exactly one definition per OS, chosen by cmake/<os>/<os>.cmake.
std::unique_ptr<WifiBackend> CreatePlatformWifiBackend();
