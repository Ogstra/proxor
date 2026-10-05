// macOS only (listed only in cmake/macos/macos.cmake, compiled with -fobjc-arc).
// Observes NSWorkspace WillSleep/DidWake; it never sleeps anything and posts nothing.
#import <AppKit/AppKit.h>

#include "sys/SleepWake.hpp"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <memory>

namespace ProxorSleepWake {
InstallResult Install(QObject *owner, std::function<void(bool)> onEvent) {
    if (!owner || !onEvent) return {false, QStringLiteral("macOS sleep/wake observer: no owner")};
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    QPointer<QObject> guard(owner);
    auto fn = std::make_shared<std::function<void(bool)>>(std::move(onEvent));
    auto deliver = [guard, fn](bool sleeping) {
        QObject *o = guard.data();
        if (!o) return;
        if (QThread::currentThread() == o->thread()) {
            (*fn)(sleeping); // WillSleep: snapshot before the Mac sleeps
        } else {
            QMetaObject::invokeMethod(
                o, [guard, fn, sleeping] { if (guard) (*fn)(sleeping); }, Qt::QueuedConnection);
        }
    };
    // queue:nil is deliberate: with the main queue the WillSleep block could run after the Mac already slept.
    id willSleep = [center addObserverForName:NSWorkspaceWillSleepNotification
                                       object:nil
                                        queue:nil
                                   usingBlock:^(NSNotification *) { deliver(true); }];
    id didWake = [center addObserverForName:NSWorkspaceDidWakeNotification
                                     object:nil
                                      queue:nil
                                 usingBlock:^(NSNotification *) { deliver(false); }];
    QObject::connect(owner, &QObject::destroyed, [willSleep, didWake] {
        NSNotificationCenter *c = [[NSWorkspace sharedWorkspace] notificationCenter];
        [c removeObserver:willSleep];
        [c removeObserver:didWake];
    });
    return {true, QStringLiteral("macOS sleep/wake: NSWorkspace WillSleep/DidWake notifications")};
}
} // namespace ProxorSleepWake
