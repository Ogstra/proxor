#include "LinuxDesktop.hpp"

#include <QStringList>
#include <QtGlobal>

namespace ProxorPlatform {

namespace {

struct TokenMatch {
    bool recognised = false;
    LinuxDesktopFamily family = LinuxDesktopFamily::Other;
    bool gnomeSettings = false;
};

TokenMatch MatchToken(const QString &rawToken) {
    const auto token = rawToken.trimmed().toLower();
    TokenMatch m;
    if (token.isEmpty()) return m;
    m.recognised = true;
    if (token == "kde" || token == "plasma") {
        m.family = LinuxDesktopFamily::Kde;
    } else if (token == "gnome" || token == "gnome-classic" || token == "gnome-flashback" || token == "gnome-xorg") {
        m.family = LinuxDesktopFamily::Gnome;
        m.gnomeSettings = true;
    } else if (token == "unity" || token == "budgie" || token == "budgie-desktop" || token == "pantheon") {
        m.family = LinuxDesktopFamily::Gnome;
        m.gnomeSettings = true;
    } else if (token == "x-cinnamon" || token == "cinnamon") {
        m.family = LinuxDesktopFamily::Cinnamon;
        m.gnomeSettings = true;
    } else if (token == "mate") {
        m.family = LinuxDesktopFamily::Mate;
        m.gnomeSettings = true;
    } else if (token == "xfce") {
        m.family = LinuxDesktopFamily::Xfce;
    } else {
        m.family = LinuxDesktopFamily::Other;
    }
    return m;
}

LinuxCompositor CompositorFromToken(const QString &rawToken) {
    const auto token = rawToken.trimmed().toLower();
    if (token == "hyprland") return LinuxCompositor::Hyprland;
    if (token == "sway") return LinuxCompositor::Sway;
    if (token == "niri") return LinuxCompositor::Niri;
    if (token == "river") return LinuxCompositor::River;
    if (token == "wayfire") return LinuxCompositor::Wayfire;
    if (token == "labwc") return LinuxCompositor::Labwc;
    return LinuxCompositor::None;
}

} // namespace

QString LinuxDesktopFamilyName(LinuxDesktopFamily family) {
    switch (family) {
    case LinuxDesktopFamily::Gnome: return QStringLiteral("GNOME");
    case LinuxDesktopFamily::Kde: return QStringLiteral("KDE Plasma");
    case LinuxDesktopFamily::Cinnamon: return QStringLiteral("Cinnamon");
    case LinuxDesktopFamily::Mate: return QStringLiteral("MATE");
    case LinuxDesktopFamily::Xfce: return QStringLiteral("Xfce");
    case LinuxDesktopFamily::Other: return QStringLiteral("other desktop");
    case LinuxDesktopFamily::Unknown: break;
    }
    return QStringLiteral("unknown desktop");
}

QString LinuxCompositorName(LinuxCompositor compositor) {
    switch (compositor) {
    case LinuxCompositor::Hyprland: return QStringLiteral("hyprland");
    case LinuxCompositor::Sway: return QStringLiteral("sway");
    case LinuxCompositor::Niri: return QStringLiteral("niri");
    case LinuxCompositor::River: return QStringLiteral("river");
    case LinuxCompositor::Wayfire: return QStringLiteral("wayfire");
    case LinuxCompositor::Labwc: return QStringLiteral("labwc");
    case LinuxCompositor::None: break;
    }
    return QString();
}

LinuxDesktopInfo DetectLinuxDesktop(const LinuxDesktopEnv &env) {
    LinuxDesktopInfo info;

    QStringList tokens = env.currentDesktop.split(':', Qt::SkipEmptyParts);
    tokens << env.sessionDesktop << env.desktopSession;

    // First pass: a recognised desktop family wins over "Other" (e.g. "ubuntu:GNOME").
    TokenMatch chosen;
    QString firstUnknown;
    for (const auto &t: tokens) {
        const auto m = MatchToken(t);
        if (!m.recognised) continue;
        if (m.family == LinuxDesktopFamily::Other) {
            if (firstUnknown.isEmpty()) firstUnknown = t.trimmed();
            continue;
        }
        chosen = m;
        break;
    }

    if (chosen.recognised) {
        info.family = chosen.family;
        info.usesGnomeProxySettings = chosen.gnomeSettings;
    } else if (!firstUnknown.isEmpty()) {
        info.family = LinuxDesktopFamily::Other;
    } else {
        info.family = LinuxDesktopFamily::Unknown;
    }

    if (info.family == LinuxDesktopFamily::Kde) {
        const int v = env.kdeSessionVersion.trimmed().toInt();
        info.kdeMajor = (v == 5 || v == 6) ? v : 0;
        info.label = info.kdeMajor != 0 ? QStringLiteral("KDE Plasma %1").arg(info.kdeMajor) : QStringLiteral("KDE Plasma");
    } else if (info.family == LinuxDesktopFamily::Other) {
        info.label = QStringLiteral("unknown desktop (%1)").arg(firstUnknown);
    } else {
        info.label = LinuxDesktopFamilyName(info.family);
    }

    // Standalone Wayland compositor (diagnostics only). A full desktop wins.
    const bool fullDesktop = info.family == LinuxDesktopFamily::Gnome || info.family == LinuxDesktopFamily::Kde
        || info.family == LinuxDesktopFamily::Cinnamon || info.family == LinuxDesktopFamily::Mate
        || info.family == LinuxDesktopFamily::Xfce;
    if (!fullDesktop) {
        QStringList names = env.currentDesktop.split(':', Qt::SkipEmptyParts);
        names << env.sessionDesktop;
        for (const auto &t: names) {
            info.compositor = CompositorFromToken(t);
            if (info.compositor != LinuxCompositor::None) break;
        }
        if (info.compositor == LinuxCompositor::None) {
            if (!env.hyprlandInstance.trimmed().isEmpty()) info.compositor = LinuxCompositor::Hyprland;
            else if (!env.niriSocket.trimmed().isEmpty()) info.compositor = LinuxCompositor::Niri;
            else if (!env.swaySock.trimmed().isEmpty()) info.compositor = LinuxCompositor::Sway;
        }
    }
    return info;
}

LinuxDesktopEnv LinuxDesktopEnvFromProcess() {
    LinuxDesktopEnv env;
    env.currentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    env.sessionDesktop = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    env.desktopSession = qEnvironmentVariable("DESKTOP_SESSION");
    env.kdeSessionVersion = qEnvironmentVariable("KDE_SESSION_VERSION");
    env.hyprlandInstance = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    env.niriSocket = qEnvironmentVariable("NIRI_SOCKET");
    env.swaySock = qEnvironmentVariable("SWAYSOCK");
    return env;
}

} // namespace ProxorPlatform
