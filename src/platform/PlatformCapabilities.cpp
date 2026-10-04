#include "platform/PlatformCapabilities.hpp"

#include <QCoreApplication>

namespace ProxorPlatform {

namespace {

QString T(const char *text) { return QCoreApplication::translate("PlatformCapabilities", text); }

CapabilityStatus Make(Support support, const QString &reason) { return CapabilityStatus{support, reason}; }
CapabilityStatus Unsupported(const QString &reason) { return Make(Support::Unsupported, reason); }
CapabilityStatus Degraded(const QString &reason) { return Make(Support::Degraded, reason); }

bool IsLinuxFlatpak(const PlatformEnvironment &env) { return env.os == HostOs::Linux && IsFlatpak(env.packageMode); }

QString NoQrReader() { return T("This build of Proxor has no QR code reader."); }
QString FlatpakTun() { return T("Tun Mode is not available in the Flatpak sandbox. These settings apply to the native packages and the AppImage."); }

// Linux backend choice shared by hotkeys and screen capture: the desktop portal where it is offered;
// the Flatpak and Wayland have no native fallback; XWayland/unknown sessions fall back to the native path.
template <typename Backend>
Backend LinuxBackend(const PlatformEnvironment &env, uint portalVersion) {
    const bool portal = portalVersion >= 1;
    if (IsFlatpak(env.packageMode) || env.session == DisplaySession::Wayland)
        return portal ? Backend::Portal : Backend::None;
    switch (env.session) {
        case DisplaySession::XWayland:
        case DisplaySession::Unknown:
            return portal ? Backend::Portal : Backend::Native;
        case DisplaySession::X11:
        case DisplaySession::NotApplicable:
            break;
    }
    return Backend::Native;
}

CapabilityStatus QueryHotkeys(const PlatformEnvironment &env) {
    if (!env.hotkeyBackendBuilt) return Unsupported(T("This build of Proxor has no global hotkey support."));
    switch (env.os) {
        case HostOs::Windows:
        case HostOs::MacOS:
            return {};
        case HostOs::Other:
            return Unsupported(T("Global hotkeys are not available on this platform."));
        case HostOs::Linux:
            break;
    }
    switch (SelectHotkeyBackend(env)) {
        case HotkeyBackend::Portal:
            return Degraded(T("Your desktop manages these hotkeys: it asks you to confirm them the first time and may assign different keys. Change them later in the desktop's keyboard shortcut settings."));
        case HotkeyBackend::None:
            if (IsFlatpak(env.packageMode))
                return Unsupported(T("Global hotkeys need the desktop's GlobalShortcuts portal (KDE Plasma 5.27 or later, GNOME 48 or later), which this desktop does not offer. The Flatpak cannot grab keys by itself."));
            return Unsupported(T("Global hotkeys in a Wayland session need the desktop's GlobalShortcuts portal (KDE Plasma 5.27 or later, GNOME 48 or later), which this desktop does not offer."));
        case HotkeyBackend::Native:
            break;
    }
    switch (env.session) {
        case DisplaySession::XWayland:
            return Degraded(T("Proxor runs through XWayland here, so hotkeys may work only while a Proxor window has focus."));
        case DisplaySession::Unknown:
            return Degraded(T("Proxor cannot tell whether this session allows global hotkeys."));
        default:
            break;
    }
    return {};
}

CapabilityStatus QueryScreenCapture(const PlatformEnvironment &env) {
    if (!env.qrReaderBuilt) return Unsupported(NoQrReader());
    switch (env.os) {
        case HostOs::Windows:
            return {};
        case HostOs::MacOS:
            return Make(Support::NeedsPermission,
                        T("Scanning the screen needs the Screen Recording permission (System Settings > Privacy & Security > Screen & System Audio Recording). Without it only the wallpaper is captured. Adding a QR code from an image file or the clipboard needs no permission."));
        case HostOs::Other:
            return Unsupported(T("Screen capture is not available on this platform. Add the QR code from an image file or the clipboard instead."));
        case HostOs::Linux:
            break;
    }
    switch (SelectScreenCaptureBackend(env)) {
        case ScreenCaptureBackend::Portal:
            return {};
        case ScreenCaptureBackend::None:
            return Unsupported(T("Screen capture needs the desktop's Screenshot portal, which this desktop does not offer. Add the QR code from an image file or the clipboard instead."));
        case ScreenCaptureBackend::Native:
            break;
    }
    switch (env.session) {
        case DisplaySession::XWayland:
            return Degraded(T("Proxor runs through XWayland here and can capture only X11 windows. If the code is not found, add it from an image file or the clipboard."));
        case DisplaySession::Unknown:
            return Degraded(T("Proxor cannot tell whether this session allows screen capture. If the code is not found, add it from an image file or the clipboard."));
        default:
            break;
    }
    return {};
}

CapabilityStatus QueryIcmp(const PlatformEnvironment &env) {
    if (env.os == HostOs::Windows || env.os == HostOs::MacOS) return {};
    return Degraded(T("ICMP ping needs unprivileged ICMP sockets, which Linux allows only when net.ipv4.ping_group_range includes your group. Otherwise Proxor measures latency with TCP ping and says so in the log."));
}

CapabilityStatus QueryAutoStart(const PlatformEnvironment &env) {
    if (env.os == HostOs::MacOS) return {}; // phase 53: user LaunchAgent / SMAppService; runtime state is shown by Settings
    if (IsLinuxFlatpak(env)) {
        if (env.backgroundPortal >= 1) return {};
        return Unsupported(T("Start with system in the Flatpak needs the desktop's Background portal, which this desktop does not offer. Add Proxor to your desktop's autostart settings instead."));
    }
    // Native Linux packages are Supported here: native packages start through the /usr/bin/proxor wrapper (52-02).
    return {};
}

CapabilityStatus QuerySsid(const PlatformEnvironment &env) {
    // Linux reads the SSID from NetworkManager (phase 51); macOS flips in plan 51-06.
    switch (env.os) {
        case HostOs::Windows:
            return {};
        case HostOs::MacOS:
            // Supported: CoreWLAN reads the SSID; the Location permission is a runtime state shown in the
            // On-Demand tab, not a static capability, so the page and the Hosts column stay editable
            // before permission is granted.
            return {};
        case HostOs::Linux:
            return {};
        case HostOs::Other:
            break;
    }
    return Unsupported(T("Wi-Fi network detection is not available on this platform, so On-Demand rules and \"Skip on SSIDs\" never trigger. Your settings are kept."));
}

CapabilityStatus QuerySystemTray(const PlatformEnvironment &env) {
    switch (env.os) {
        case HostOs::Windows:
        case HostOs::MacOS:
            return {};
        case HostOs::Other:
            return Unsupported(T("This platform has no system tray support."));
        case HostOs::Linux:
            break;
    }
    if (env.trayAvailable) return {};
    if (IsFlatpak(env.packageMode))
        return Unsupported(T("The Flatpak cannot reach a system tray on this desktop. Proxor keeps its window open (minimized) instead of hiding it. On GNOME, install the \"AppIndicator and KStatusNotifierItem Support\" extension."));
    if (env.desktop == LinuxDesktopFamily::Gnome)
        return Unsupported(T("GNOME shows tray icons only with the \"AppIndicator and KStatusNotifierItem Support\" extension. Without a tray Proxor keeps its window open (minimized) instead of hiding it."));
    return Unsupported(T("This desktop has no system tray. Proxor keeps its window open (minimized) instead of hiding it."));
}

QString DesktopName(LinuxDesktopFamily family) {
    switch (family) {
        case LinuxDesktopFamily::Unknown: return QStringLiteral("unknown");
        case LinuxDesktopFamily::Gnome: return QStringLiteral("gnome");
        case LinuxDesktopFamily::Kde: return QStringLiteral("kde");
        case LinuxDesktopFamily::Cinnamon: return QStringLiteral("cinnamon");
        case LinuxDesktopFamily::Mate: return QStringLiteral("mate");
        case LinuxDesktopFamily::Xfce: return QStringLiteral("xfce");
        case LinuxDesktopFamily::Other: return QStringLiteral("other");
    }
    return QStringLiteral("unknown");
}

} // namespace

HostOs CompiledHostOs() {
#if defined(Q_OS_WIN)
    return HostOs::Windows;
#elif defined(Q_OS_MACOS)
    return HostOs::MacOS;
#elif defined(Q_OS_LINUX)
    return HostOs::Linux;
#else
    return HostOs::Other;
#endif
}

DisplaySession SessionFromEnvironment(HostOs os, const QString &qpaPlatformName, bool waylandDisplaySet) {
    if (os != HostOs::Linux) return DisplaySession::NotApplicable;
    if (qpaPlatformName.startsWith(QStringLiteral("wayland"))) return DisplaySession::Wayland;
    if (qpaPlatformName == QStringLiteral("xcb")) return waylandDisplaySet ? DisplaySession::XWayland : DisplaySession::X11;
    return DisplaySession::Unknown;
}

CapabilityStatus QueryCapability(Capability capability, const PlatformEnvironment &env) {
    switch (capability) {
        case Capability::GlobalHotkeys:
            return QueryHotkeys(env);
        case Capability::ScreenQrCapture:
            return QueryScreenCapture(env);
        case Capability::QrImageImport:
            if (!env.qrReaderBuilt) return Unsupported(NoQrReader());
            return {};
        case Capability::IcmpPing:
            return QueryIcmp(env);
        case Capability::AutoStart:
            return QueryAutoStart(env);
        case Capability::OnDemandSsid:
            return QuerySsid(env);
        case Capability::TunMode:
            if (IsLinuxFlatpak(env)) return Unsupported(FlatpakTun());
            return {};
        case Capability::TunStrictRoute:
            if (env.os == HostOs::MacOS) return Unsupported(T("Strict route has no effect on macOS: Tun runs in the Proxor service."));
            if (IsLinuxFlatpak(env)) return Unsupported(FlatpakTun());
            return {};
        case Capability::TunSingleCore:
            if (env.os == HostOs::MacOS) return Unsupported(T("On macOS Tun always runs in the Proxor service."));
            if (IsLinuxFlatpak(env)) return Unsupported(FlatpakTun());
            if (env.os == HostOs::Linux && env.packageMode == PackageMode::AppImage)
                return Unsupported(T("The AppImage always runs Tun as a separate privileged process; single-core Tun needs a native package."));
            return {};
        case Capability::SystemTray:
            return QuerySystemTray(env);
    }
    return {};
}

HotkeyBackend SelectHotkeyBackend(const PlatformEnvironment &env) {
    if (!env.hotkeyBackendBuilt) return HotkeyBackend::None;
    switch (env.os) {
        case HostOs::Windows:
        case HostOs::MacOS:
            return HotkeyBackend::Native;
        case HostOs::Other:
            return HotkeyBackend::None;
        case HostOs::Linux:
            break;
    }
    return LinuxBackend<HotkeyBackend>(env, env.globalShortcutsPortal);
}

ScreenCaptureBackend SelectScreenCaptureBackend(const PlatformEnvironment &env) {
    if (!env.qrReaderBuilt) return ScreenCaptureBackend::None;
    switch (env.os) {
        case HostOs::Windows:
        case HostOs::MacOS:
            return ScreenCaptureBackend::Native;
        case HostOs::Other:
            return ScreenCaptureBackend::None;
        case HostOs::Linux:
            break;
    }
    return LinuxBackend<ScreenCaptureBackend>(env, env.screenshotPortal);
}

bool IsUsable(const CapabilityStatus &status) { return status.support != Support::Unsupported; }

QString CapabilityName(Capability capability) {
    switch (capability) {
        case Capability::GlobalHotkeys: return QStringLiteral("global-hotkeys");
        case Capability::ScreenQrCapture: return QStringLiteral("screen-qr-capture");
        case Capability::QrImageImport: return QStringLiteral("qr-image-import");
        case Capability::IcmpPing: return QStringLiteral("icmp-ping");
        case Capability::AutoStart: return QStringLiteral("autostart");
        case Capability::OnDemandSsid: return QStringLiteral("on-demand-ssid");
        case Capability::TunMode: return QStringLiteral("tun-mode");
        case Capability::TunStrictRoute: return QStringLiteral("tun-strict-route");
        case Capability::TunSingleCore: return QStringLiteral("tun-single-core");
        case Capability::SystemTray: return QStringLiteral("system-tray");
    }
    return QStringLiteral("unknown");
}

QString DescribePlatformEnvironment(const PlatformEnvironment &env) {
    QString os;
    switch (env.os) {
        case HostOs::Windows: os = QStringLiteral("windows"); break;
        case HostOs::Linux: os = QStringLiteral("linux"); break;
        case HostOs::MacOS: os = QStringLiteral("macos"); break;
        case HostOs::Other: os = QStringLiteral("other"); break;
    }
    QString session;
    switch (env.session) {
        case DisplaySession::NotApplicable: session = QStringLiteral("not-applicable"); break;
        case DisplaySession::X11: session = QStringLiteral("x11"); break;
        case DisplaySession::XWayland: session = QStringLiteral("xwayland"); break;
        case DisplaySession::Wayland: session = QStringLiteral("wayland"); break;
        case DisplaySession::Unknown: session = QStringLiteral("unknown"); break;
    }
    return QStringLiteral("os=%1 package=%2 session=%3 tray=%4 desktop=%5 portals=background:%6,screenshot:%7,shortcuts:%8")
        .arg(os, PackageModeName(env.packageMode), session, env.trayAvailable ? QStringLiteral("yes") : QStringLiteral("no"),
             DesktopName(env.desktop))
        .arg(env.backgroundPortal)
        .arg(env.screenshotPortal)
        .arg(env.globalShortcutsPortal);
}

QList<Capability> AllCapabilities() {
    return {Capability::GlobalHotkeys, Capability::ScreenQrCapture, Capability::QrImageImport,
            Capability::IcmpPing,      Capability::AutoStart,       Capability::OnDemandSsid,
            Capability::TunMode,       Capability::TunStrictRoute,  Capability::TunSingleCore,
            Capability::SystemTray};
}

} // namespace ProxorPlatform
