// src/ui/mac/MacPlatform.h — macOS-only helpers. Include only under #ifdef Q_OS_MACOS.
#pragma once
#include <functional>
class QIcon; class QMenu; class QObject; class QString;
namespace ProxorMac {
// App-owned NSStatusItem, used on macOS instead of QSystemTrayIcon's status item.
// Qt 6.11's QCocoaSystemTrayIcon::emitActivated() reads NSApp.currentEvent.clickCount
// from its menu-tracking observer and button action; on macOS 27 that event is a
// SysDefined event and -clickCount throws (see quick 260928-ncf tray-crash.log).
class StatusItem final {
public:
    StatusItem();
    ~StatusItem();                        // removes the item from NSStatusBar
    StatusItem(const StatusItem &) = delete;
    StatusItem &operator=(const StatusItem &) = delete;
    void setMenu(QMenu *menu);            // statusItem.menu = menu->toNSMenu()
    void setIcon(const QIcon &icon);      // QIcon -> NSImage fitted to the status bar thickness, Retina-aware
    // true (default): the status-colored icon as is. false: a monochrome template image (black
    // glyph + alpha only) that macOS tints for light/dark menu bars. State stays readable there:
    // saturated (colored) parts are drawn solid, the unsaturated/white parts of the idle icon are
    // drawn as a faint ghost, so "running" = both arcs solid, "stopped" = one solid, one faint.
    // Re-renders the last icon immediately.
    void setColored(bool colored);
    void setToolTip(const QString &text); // statusItem.button.toolTip
    void setVisible(bool visible);        // statusItem.visible
    [[nodiscard]] bool isVisible() const;
private:
    struct Impl;
    Impl *d;
};
// Filters platform-originated (spontaneous) QEvent::Quit on qApp: Cmd+Q from Qt's default app-menu
// Quit item, Dock > Quit, quit Apple events, logout. It calls event->ignore() AND returns true
// (so applicationShouldTerminate answers NSTerminateCancel), then queues onQuit on the event loop.
// Non-spontaneous Quit (QCoreApplication::quit()) passes through untouched.
void InstallQuitInterceptor(QObject *owner, std::function<void()> onQuit);
// Call right before the final QCoreApplication::quit() of the real exit path. Qt's quit() ends up in
// -[NSApp terminate:], which sends one more *spontaneous* QEvent::Quit; once this has been called the
// interceptor lets every Quit through so the process can actually exit instead of looping.
void AllowQuit();
}
