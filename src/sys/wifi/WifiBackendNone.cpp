#include "sys/wifi/WifiBackend.hpp"

#include <QCoreApplication>

namespace {

class UnavailableWifiBackend final : public WifiBackend {
public:
    ProxorWifi::WifiReading read() override {
        return ProxorWifi::Unavailable(
            QCoreApplication::translate("WifiSsid", "Wi-Fi network detection is not available on this platform yet."),
            "none");
    }
};

} // namespace

std::unique_ptr<WifiBackend> CreatePlatformWifiBackend() {
    return std::make_unique<UnavailableWifiBackend>();
}
