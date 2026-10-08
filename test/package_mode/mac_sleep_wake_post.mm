// Test-only helper: posts the NSWorkspace sleep/wake notifications into this process's own
// notification center. It never sleeps the Mac and has no effect on the system.
#import <AppKit/AppKit.h>

void ProxorTestPostWorkspaceSleepWake(bool sleeping) {
    NSWorkspace *ws = [NSWorkspace sharedWorkspace];
    [[ws notificationCenter] postNotificationName:(sleeping ? NSWorkspaceWillSleepNotification
                                                            : NSWorkspaceDidWakeNotification)
                                           object:ws];
}
