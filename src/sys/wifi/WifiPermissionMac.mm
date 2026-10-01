#include "sys/wifi/WifiPermission.hpp"

#include "platform/WifiMacClassify.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <atomic>
#include <utility>
#include <vector>

#import <AppKit/AppKit.h>
#import <CoreLocation/CoreLocation.h>
#import <dispatch/dispatch.h>

// macOS hides the Wi-Fi network name unless the app is authorized for Location Services, so the
// permission hooks are CLLocationManager calls.
//
// Threading: the CLLocationManager is created lazily, on the Cocoa main thread only (the Qt UI
// thread). CurrentWifiPermission() may run from the MainWindow constructor before the Qt event
// loop starts: creating the manager and reading authorizationStatus needs no running loop; the
// delegate callback arrives later once the loop runs, and a RequestWifiPermission queued before
// then is delivered after. If a hook is called off the main thread, the manager work is
// dispatched to the main queue instead of creating the manager there.
//
// [CLLocationManager locationServicesEnabled] warns about blocking the main thread, so its result
// is cached (default true until known) and refreshed on a background queue at startup and on every
// authorization change.

namespace {

using Pending = std::pair<QPointer<QObject>, std::function<void(ProxorWifi::PermissionState)>>;

std::vector<Pending> &PendingList() {
    static std::vector<Pending> list;
    return list;
}

std::atomic<bool> gServicesEnabled{true};

void RefreshServicesEnabled(void (^completion)(void)) {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
        gServicesEnabled.store([CLLocationManager locationServicesEnabled]);
        if (completion) dispatch_async(dispatch_get_main_queue(), completion);
    });
}

} // namespace

@interface ProxorLocationDelegate : NSObject <CLLocationManagerDelegate>
@property(nonatomic, strong) CLLocationManager *manager;
+ (instancetype)shared;
- (void)flushIfDecided;
@end

static ProxorWifi::PermissionState LocationState(CLLocationManager *manager) {
    return ProxorWifi::MapMacLocationStatus(static_cast<int>(manager.authorizationStatus), gServicesEnabled.load());
}

@implementation ProxorLocationDelegate

+ (instancetype)shared {
    // Main thread only (callers guarantee it).
    static ProxorLocationDelegate *instance = nil;
    if (!instance) {
        instance = [[ProxorLocationDelegate alloc] init];
        instance.manager = [[CLLocationManager alloc] init];
        instance.manager.delegate = instance;
        RefreshServicesEnabled(nil);
    }
    return instance;
}

- (void)flushIfDecided {
    ProxorWifi::PermissionState state = LocationState(self.manager);
    if (state == ProxorWifi::PermissionState::NotDetermined) return;
    std::vector<Pending> ready;
    ready.swap(PendingList());
    for (auto &entry : ready) {
        QObject *context = entry.first.data();
        if (!context) continue;
        auto done = entry.second;
        QMetaObject::invokeMethod(context, [done, state] { done(state); }, Qt::QueuedConnection);
    }
}

- (void)locationManagerDidChangeAuthorization:(CLLocationManager *)manager {
    (void)manager;
    ProxorLocationDelegate *me = self;
    RefreshServicesEnabled(^{ [me flushIfDecided]; });
}

- (void)locationManager:(CLLocationManager *)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    (void)manager;
    (void)status;
    ProxorLocationDelegate *me = self;
    RefreshServicesEnabled(^{ [me flushIfDecided]; });
}

- (void)locationManager:(CLLocationManager *)manager didFailWithError:(NSError *)error {
    (void)manager;
    (void)error; // kCLErrorDomain 1 while the prompt is pending; the authorization callback decides.
}

@end

ProxorWifi::PermissionState ProxorWifi::CurrentWifiPermission() {
    if (![NSThread isMainThread]) {
        __block PermissionState state = PermissionState::NotDetermined;
        dispatch_sync(dispatch_get_main_queue(), ^{ state = CurrentWifiPermission(); });
        return state;
    }
    return LocationState([ProxorLocationDelegate shared].manager);
}

void ProxorWifi::RequestWifiPermission(QObject *context, std::function<void(PermissionState)> done) {
    if (!context || !done) return;
    QPointer<QObject> guard(context);
    auto work = [guard, done] {
        if (!guard) return;
        ProxorLocationDelegate *delegate = [ProxorLocationDelegate shared];
        PermissionState state = LocationState(delegate.manager);
        if (state != PermissionState::NotDetermined) {
            QMetaObject::invokeMethod(guard.data(), [done, state] { done(state); }, Qt::QueuedConnection);
            return;
        }
        PendingList().emplace_back(guard, done);
        [delegate.manager requestWhenInUseAuthorization];
    };
    if ([NSThread isMainThread]) work();
    else dispatch_async(dispatch_get_main_queue(), ^{ work(); });
}

bool ProxorWifi::OpenWifiPermissionSettings() {
    @autoreleasepool {
        NSArray<NSString *> *urls = @[
            @"x-apple.systempreferences:com.apple.preference.security?Privacy_LocationServices",
            @"x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_LocationServices",
        ];
        for (NSString *s in urls) {
            NSURL *url = [NSURL URLWithString:s];
            if (url && [[NSWorkspace sharedWorkspace] openURL:url]) return true;
        }
    }
    return false;
}
