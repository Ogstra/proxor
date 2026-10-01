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
    if (IsFlatpak(env.packageMode))
        return Unsupported(T("Global hotkeys are not available in the Flatpak: the sandbox cannot grab keys, and the desktop GlobalShortcuts portal is not supported yet."));
    switch (env.session) {
        case DisplaySession::Wayland:
            return Unsupported(T("Global hotkeys are not available in a Wayland session: Proxor can register them only under X11."));
        case DisplaySession::XWayland:
            return Degraded(T("Proxor runs through XWayland here, so hotkeys may work only while a Proxor window has focus."));
        case DisplaySession::Unknown:
            return Degraded(T("Proxor cannot tell whether this session allows global hotkeys."));
        case DisplaySession::X11:
        case DisplaySession::NotApplicable:
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
    if (IsFlatpak(env.packageMode))
        return Unsupported(T("Screen capture is not available in the Flatpak. Add the QR code from an image file or the clipboard instead."));
    switch (env.session) {
        case DisplaySession::Wayland:
            return Unsupported(T("Screen capture is not available in a Wayland session. Add the QR code from an image file or the clipboard instead."));
        case DisplaySession::XWayland:
            return Degraded(T("Proxor runs through XWayland here and can capture only X11 windows. If the code is not found, add it from an image file or the clipboard."));
        case DisplaySession::Unknown:
            return Degraded(T("Proxor cannot tell whether this session allows screen capture. If the code is not found, add it from an image file or the clipboard."));
        case DisplaySession::X11:
        case DisplaySession::NotApplicable:
            break;
    }
    return {};
}

CapabilityStatus QueryIcmp(const PlatformEnvironment &env) {
    if (env.os == HostOs::Windows || env.os == HostOs::MacOS) return {};
    return Degraded(T("ICMP ping needs unprivileged ICMP sockets, which Linux allows only when net.ipv4.ping_group_range includes your group. Otherwise Proxor measures latency with TCP ping and says so in the log."));
}

CapabilityStatus QueryAutoStart(const PlatformEnvironment &env) {
    if (env.os == HostOs::MacOS)
        return Unsupported(T("Start with system is not available on macOS yet. Add Proxor in System Settings > General > Login Items instead."));
    if (IsLinuxFlatpak(env))
        return Unsupported(T("Start with system is not available in the Flatpak yet. Add Proxor to your desktop's autostart settings instead."));
    // Native Linux packages are Supported here; the native-package Exec bug (G-02) is phase 52's fix.
    return {};
}

CapabilityStatus QuerySsid(const PlatformEnvironment &env) {
    // Linux reads the SSID from NetworkManager (phase 51); macOS flips in plan 51-06.
    switch (env.os) {
        case HostOs::Windows:
            return {};
        case HostOs::MacOS:
            return Unsupported(T("Wi-Fi network detection is not available on macOS yet, so On-Demand rules and \"Skip on SSIDs\" never trigger. Your settings are kept."));
        case HostOs::Linux:
            return {};
        case HostOs::Other:
            break;
    }
    return Unsupported(T("Wi-Fi network detection is not available on this platform, so On-Demand rules and \"Skip on SSIDs\" never trigger. Your settings are kept."));
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
    }
    return {};
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
    return QStringLiteral("os=%1 package=%2 session=%3").arg(os, PackageModeName(env.packageMode), session);
}

QList<Capability> AllCapabilities() {
    return {Capability::GlobalHotkeys, Capability::ScreenQrCapture, Capability::QrImageImport,
            Capability::IcmpPing,      Capability::AutoStart,       Capability::OnDemandSsid,
            Capability::TunMode,       Capability::TunStrictRoute,  Capability::TunSingleCore};
}

} // namespace ProxorPlatform
