#pragma once

// Single truth table for platform-dependent features: what works, what is degraded, what needs an OS
// permission and what is unavailable, with a user-facing reason for every non-Supported answer.
// Qt Core only (no GUI or widget classes, no app globals) so it is unit-tested on every CI runner.

#include "main/PackageMode.hpp"
#include "platform/LinuxDesktop.hpp"

#include <QList>
#include <QString>

namespace ProxorPlatform {

enum class HostOs { Windows, Linux, MacOS, Other };
enum class DisplaySession { NotApplicable, X11, XWayland, Wayland, Unknown };

struct PlatformEnvironment {
    HostOs os = HostOs::Other;
    PackageMode packageMode = PackageMode::NativeOrPortable;
    DisplaySession session = DisplaySession::NotApplicable;
    bool hotkeyBackendBuilt = true; // false in NKR_NO_QHOTKEY builds
    bool qrReaderBuilt = true;      // false in NKR_NO_ZXING builds
    bool trayAvailable = true;      // QSystemTrayIcon::isSystemTrayAvailable() in the app
    LinuxDesktopFamily desktop = LinuxDesktopFamily::Unknown; // Linux only; Unknown elsewhere
    uint backgroundPortal = 0;      // ProxorDesktop::Portals().background (0 = absent)
    uint screenshotPortal = 0;
    uint globalShortcutsPortal = 0;
};

enum class HotkeyBackend { None, Native, Portal };
enum class ScreenCaptureBackend { None, Native, Portal };

enum class Capability {
    GlobalHotkeys,
    ScreenQrCapture,
    QrImageImport,
    IcmpPing,
    AutoStart,
    OnDemandSsid,
    TunMode,
    TunStrictRoute,
    TunSingleCore,
    SystemTray
};

enum class Support { Supported, Degraded, NeedsPermission, Unsupported };

struct CapabilityStatus {
    Support support = Support::Supported;
    QString reason;
};

HostOs CompiledHostOs();
DisplaySession SessionFromEnvironment(HostOs os, const QString &qpaPlatformName, bool waylandDisplaySet);
CapabilityStatus QueryCapability(Capability capability, const PlatformEnvironment &env);
HotkeyBackend SelectHotkeyBackend(const PlatformEnvironment &env);
ScreenCaptureBackend SelectScreenCaptureBackend(const PlatformEnvironment &env);
bool IsUsable(const CapabilityStatus &status);
QString CapabilityName(Capability capability);
QString DescribePlatformEnvironment(const PlatformEnvironment &env);
QList<Capability> AllCapabilities();

} // namespace ProxorPlatform
