#include "sys/wifi/WifiBackend.hpp"

#include <QCoreApplication>
#include <QProcess>

namespace {

class NetshWifiBackend final : public WifiBackend {
public:
    ProxorWifi::WifiReading read() override {
        QProcess p;
        p.start("netsh", {"wlan", "show", "interfaces"});
        if (!p.waitForStarted(2000)) {
            return ProxorWifi::Unavailable(QCoreApplication::translate("WifiSsid", "netsh could not be started."),
                                           "netsh");
        }
        if (!p.waitForFinished(3000)) {
            p.kill();
            p.waitForFinished(500);
            return ProxorWifi::Unavailable(
                QCoreApplication::translate("WifiSsid", "netsh did not answer within 3 seconds."), "netsh");
        }
        return ProxorWifi::ParseNetshInterfaces(QString::fromLocal8Bit(p.readAllStandardOutput()));
    }
};

} // namespace

std::unique_ptr<WifiBackend> CreatePlatformWifiBackend() {
    return std::make_unique<NetshWifiBackend>();
}
