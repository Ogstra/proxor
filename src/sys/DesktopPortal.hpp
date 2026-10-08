#pragma once
// OS-neutral desktop portal facade. Linux implements it over QtDBus (src/sys/linux/*);
// Windows and macOS link DesktopPortalNone.cpp, which reports every portal absent.
#include <QList>
#include <QString>
#include <QStringList>
#include <functional>
#include <memory>
class QObject;

namespace ProxorDesktop {

struct PortalVersions {
    uint background = 0;      // org.freedesktop.portal.Background "version"; 0 = absent
    uint screenshot = 0;      // org.freedesktop.portal.Screenshot
    uint globalShortcuts = 0; // org.freedesktop.portal.GlobalShortcuts
    QString detail;           // for the log: why everything is 0
};
void StartPortalProbe();         // idempotent; Linux: probe on a worker thread; None: no-op. Call early in main().
const PortalVersions &Portals(); // cached result; waits for a running probe at most 1500 ms since StartPortalProbe

enum class PortalOutcome { Granted, Cancelled, Denied, Failed, TimedOut, NotAvailable };
struct PortalResult {
    PortalOutcome outcome = PortalOutcome::NotAvailable;
    QString detail; // user-facing reason when not Granted (English)
};

struct AutostartResult { PortalResult result; bool autostart = false; };
// Background portal: commandline is the command INSIDE the sandbox (e.g. {"proxor", "-tray"}).
void RequestAutostart(bool enable, const QStringList &commandline, const QString &reason, QObject *context,
                      std::function<void(const AutostartResult &)> done);

struct ScreenshotResult { PortalResult result; QString imagePath; bool temporary = false; }; // temporary: safe to delete
void TakeScreenshot(const QString &parentWindow, QObject *context, std::function<void(const ScreenshotResult &)> done);

struct ShortcutRequest { QString id; QString description; QString preferredTrigger; };
struct BoundShortcut { QString id; QString triggerDescription; };
class GlobalShortcutSession {
public:
    virtual ~GlobalShortcutSession() = default; // closes the portal session asynchronously
    virtual void bind(const QList<ShortcutRequest> &shortcuts, const QString &parentWindow,
                      std::function<void(const QList<BoundShortcut> &bound, const PortalResult &result)> done) = 0;
    std::function<void(const QString &id)> onActivated;
};
std::unique_ptr<GlobalShortcutSession> CreateGlobalShortcutSession(QObject *context); // nullptr when absent

} // namespace ProxorDesktop
