#pragma once

#include <QString>

// Shared Linux desktop detection (Qt Core only, pure). Used by the system proxy planner and the tray logic.
// The standalone Wayland compositor is reported for the log's Platform line only: no decision reads it.
namespace ProxorPlatform {

enum class LinuxDesktopFamily { Unknown, Gnome, Kde, Cinnamon, Mate, Xfce, Other };

enum class LinuxCompositor { None, Hyprland, Sway, Niri, River, Wayfire, Labwc };

struct LinuxDesktopEnv {
    QString currentDesktop;   // XDG_CURRENT_DESKTOP (colon separated list)
    QString sessionDesktop;   // XDG_SESSION_DESKTOP
    QString desktopSession;   // DESKTOP_SESSION
    QString kdeSessionVersion; // KDE_SESSION_VERSION
    QString hyprlandInstance;  // HYPRLAND_INSTANCE_SIGNATURE
    QString niriSocket;        // NIRI_SOCKET
    QString swaySock;          // SWAYSOCK
};

struct LinuxDesktopInfo {
    LinuxDesktopFamily family = LinuxDesktopFamily::Unknown;
    int kdeMajor = 0;                    // 5 or 6 when known, else 0
    bool usesGnomeProxySettings = false; // gsettings org.gnome.system.proxy is honoured
    QString label;                       // "KDE Plasma 6", "GNOME", "unknown desktop (sway)"
    LinuxCompositor compositor = LinuxCompositor::None; // diagnostics only
};

LinuxDesktopInfo DetectLinuxDesktop(const LinuxDesktopEnv &env);
LinuxDesktopEnv LinuxDesktopEnvFromProcess();
QString LinuxDesktopFamilyName(LinuxDesktopFamily family);
QString LinuxCompositorName(LinuxCompositor compositor);

} // namespace ProxorPlatform
