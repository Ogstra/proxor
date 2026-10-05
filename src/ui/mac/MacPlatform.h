// src/ui/mac/MacPlatform.h — macOS-only helpers. Include only under #ifdef Q_OS_MACOS.
#pragma once
#include <functional>
class QIcon; class QMenu; class QObject; class QPoint; class QString; class QWidget;
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
    // Monochrome only: false makes the system draw the glyph dimmed (NSStatusBarButton.appearsDisabled),
    // true draws it at full strength, like other VPN apps (dim = disconnected, bright = connected).
    // Default true.
    void setActive(bool active);
    // Two-line text (e.g. "12.0K/s↑\n16.0K/s↓") drawn to the right of the icon, like other VPN apps;
    // an empty string removes it and the item goes back to a square icon-only item.
    void setSpeedText(const QString &text);
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
// Calls onReopen on the UI thread each time the user reopens Proxor (Dock icon click, `open -a` on the
// running app): an NSAppleEventManager handler for kAEReopenApplication, installed once the event loop runs.
// Never fires at launch. onReopen decides whether anything must be shown.
void InstallReopenHandler(QObject *owner, std::function<void()> onReopen);

// Pops `menu` up as a native NSMenu just below `anchor` (a widget of a visible window) and returns
// when it is dismissed. Replaces QMenu::popup for toolbar buttons so the menu looks and behaves
// like the menu-bar menus (rounded, shortcuts, submenu arrows).
void PopupMenu(QMenu *menu, QWidget *anchor);
// Same, at `posInAnchor` (anchor coordinates), for context menus.
void PopupMenuAt(QMenu *menu, QWidget *anchor, const QPoint &posInAnchor);
}
