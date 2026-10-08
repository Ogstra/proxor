// macOS Wi-Fi reader: CoreWLAN in-process (no helper processes). The Location permission flow is separate.
#import <CoreWLAN/CoreWLAN.h>
#import <Foundation/Foundation.h>

#include "platform/WifiMacClassify.hpp"
#include "sys/wifi/WifiBackend.hpp"

#include <QString>

namespace {

class CoreWlanWifiBackend final : public WifiBackend {
public:
    ProxorWifi::WifiReading read() override {
        @autoreleasepool {
            ProxorWifi::MacWifiSnapshot snapshot;
            CWInterface *itf = [[CWWiFiClient sharedWiFiClient] interface];
            snapshot.hasInterface = itf != nil;
            if (itf) {
                snapshot.powerOn = [itf powerOn];
                NSString *ssid = [itf ssid];
                if (ssid) snapshot.ssid = QString::fromNSString(ssid);
                // Readable without Location authorization (51-RESEARCH contract).
                snapshot.associated = snapshot.powerOn && [itf rssiValue] != 0 && [itf wlanChannel] != nil;
            }
            return ProxorWifi::ClassifyMacWifi(snapshot, true);
        }
    }
};

} // namespace

std::unique_ptr<WifiBackend> CreatePlatformWifiBackend() {
    return std::make_unique<CoreWlanWifiBackend>();
}
