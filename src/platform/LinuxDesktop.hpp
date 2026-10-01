#pragma once

#include <QString>

// Shared Linux desktop detection (Qt Core only, pure). Used by the system proxy planner and the tray logic.
namespace ProxorPlatform {

enum class LinuxDesktopFamily { Unknown, Gnome, Kde, Cinnamon, Mate, Xfce, Other };

struct LinuxDesktopEnv {
    QString currentDesktop;   // XDG_CURRENT_DESKTOP (colon separated list)
    QString sessionDesktop;   // XDG_SESSION_DESKTOP
    QString desktopSession;   // DESKTOP_SESSION
    QString kdeSessionVersion; // KDE_SESSION_VERSION
};

struct LinuxDesktopInfo {
    LinuxDesktopFamily family = LinuxDesktopFamily::Unknown;
    int kdeMajor = 0;                    // 5 or 6 when known, else 0
    bool usesGnomeProxySettings = false; // gsettings org.gnome.system.proxy is honoured
    QString label;                       // "KDE Plasma 6", "GNOME", "unknown desktop (sway)"
};

LinuxDesktopInfo DetectLinuxDesktop(const LinuxDesktopEnv &env);
LinuxDesktopEnv LinuxDesktopEnvFromProcess();
QString LinuxDesktopFamilyName(LinuxDesktopFamily family);

} // namespace ProxorPlatform
