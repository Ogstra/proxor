#include "sys/macos/MacScreenCapture.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>

namespace ProxorMac {

bool ScreenCapturePreflight() {
    return CGPreflightScreenCaptureAccess();
}

bool ScreenCaptureRequest() {
    return CGRequestScreenCaptureAccess();
}

bool OpenScreenRecordingSettings() {
    // Newer pane identifier first, as in the research order; the older one is the fallback.
    NSArray<NSString *> *urls = @[
        @"x-apple.systempreferences:com.apple.preference.security?Privacy_ScreenCapture",
        @"x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension?Privacy_ScreenCapture",
    ];
    for (NSString *s in urls) {
        NSURL *url = [NSURL URLWithString:s];
        if (url && [[NSWorkspace sharedWorkspace] openURL:url]) return true;
    }
    return false;
}

} // namespace ProxorMac
