#include "platform/PlatformCapabilities.hpp"

#include <QtTest>

using namespace ProxorPlatform;

namespace {
PlatformEnvironment Env(HostOs os, PackageMode mode, DisplaySession session) {
    PlatformEnvironment env;
    env.os = os;
    env.packageMode = mode;
    env.session = session;
    return env;
}

const PackageMode kAllModes[] = {PackageMode::NativeOrPortable, PackageMode::Winget, PackageMode::Flatpak,
                                 PackageMode::AppImage,         PackageMode::Deb,    PackageMode::Rpm,
                                 PackageMode::Arch,             PackageMode::NativeUnknownManager};
const DisplaySession kAllSessions[] = {DisplaySession::NotApplicable, DisplaySession::X11, DisplaySession::XWayland,
                                       DisplaySession::Wayland, DisplaySession::Unknown};
const HostOs kAllOs[] = {HostOs::Windows, HostOs::Linux, HostOs::MacOS, HostOs::Other};

CapabilityStatus Q(Capability c, const PlatformEnvironment &env) { return QueryCapability(c, env); }
} // namespace

class PlatformCapabilitiesTest : public QObject {
    Q_OBJECT
private slots:
    void windowsReferenceIsUnchanged() {
        for (auto mode : {PackageMode::NativeOrPortable, PackageMode::Winget}) {
            for (auto c : AllCapabilities()) {
                auto s = Q(c, Env(HostOs::Windows, mode, DisplaySession::NotApplicable));
                QCOMPARE(s.support, Support::Supported);
                QVERIFY2(s.reason.isEmpty(), qPrintable(CapabilityName(c)));
            }
        }
    }

    void linuxX11NativeSupportsHotkeysAndScreenCapture() {
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11);
        QCOMPARE(Q(Capability::GlobalHotkeys, env).support, Support::Supported);
        QCOMPARE(Q(Capability::ScreenQrCapture, env).support, Support::Supported);
    }

    void linuxWaylandDisablesHotkeysAndScreenCapture() {
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland);
        auto h = Q(Capability::GlobalHotkeys, env);
        QCOMPARE(h.support, Support::Unsupported);
        QVERIFY(h.reason.contains("Wayland"));
        QVERIFY(h.reason.contains("GNOME 48"));
        auto sc = Q(Capability::ScreenQrCapture, env);
        QCOMPARE(sc.support, Support::Unsupported);
        QVERIFY(sc.reason.contains("Screenshot portal"));
    }

    void flatpakDisablesHotkeysScreenAutostartTunStrictRouteAndSingleCore() {
        auto env = Env(HostOs::Linux, PackageMode::Flatpak, DisplaySession::X11);
        for (auto c : {Capability::GlobalHotkeys, Capability::ScreenQrCapture, Capability::AutoStart,
                       Capability::TunMode, Capability::TunStrictRoute, Capability::TunSingleCore}) {
            auto s = Q(c, env);
            QCOMPARE(s.support, Support::Unsupported);
            QVERIFY2(!s.reason.isEmpty(), qPrintable(CapabilityName(c)));
        }
        QCOMPARE(Q(Capability::QrImageImport, env).support, Support::Supported);
    }

    void xwaylandIsDegradedForHotkeysAndScreenCapture() {
        auto env = Env(HostOs::Linux, PackageMode::AppImage, DisplaySession::XWayland);
        QCOMPARE(Q(Capability::GlobalHotkeys, env).support, Support::Degraded);
        QCOMPARE(Q(Capability::ScreenQrCapture, env).support, Support::Degraded);
    }

    void macosScreenCaptureNeedsPermission() {
        auto s = Q(Capability::ScreenQrCapture, Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable));
        QCOMPARE(s.support, Support::NeedsPermission);
        QVERIFY(s.reason.contains("Screen Recording"));
    }
    void macosAutostartAnswer() {
        auto s = Q(Capability::AutoStart, Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable));
        QCOMPARE(s.support, Support::Supported);
        QVERIFY(s.reason.isEmpty());
    }
    void macosStrictRouteUnsupported() {
        auto env = Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable);
        QCOMPARE(Q(Capability::TunStrictRoute, env).support, Support::Unsupported);
    }
    void macosSingleCoreUnsupported() {
        auto env = Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable);
        QCOMPARE(Q(Capability::TunSingleCore, env).support, Support::Unsupported);
    }
    void macosHotkeysSupported() {
        auto env = Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable);
        QCOMPARE(Q(Capability::GlobalHotkeys, env).support, Support::Supported);
    }

    void onDemandSsidPerPlatform() {
        for (auto mode : kAllModes)
            for (auto session : kAllSessions) {
                auto s = Q(Capability::OnDemandSsid, Env(HostOs::Linux, mode, session));
                QCOMPARE(s.support, Support::Supported);
                QVERIFY(s.reason.isEmpty());
            }
        auto mac = Q(Capability::OnDemandSsid, Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable));
        QCOMPARE(mac.support, Support::Supported);
        QVERIFY(mac.reason.isEmpty());
        QCOMPARE(Q(Capability::OnDemandSsid, Env(HostOs::Windows, PackageMode::NativeOrPortable, DisplaySession::NotApplicable)).support,
                 Support::Supported);
    }

    void icmpPingDegradedOnLinuxSupportedOnWindowsAndMacos() {
        auto s = Q(Capability::IcmpPing, Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11));
        QCOMPARE(s.support, Support::Degraded);
        QVERIFY(s.reason.contains("ping_group_range"));
        QVERIFY(s.reason.contains("TCP"));
        QCOMPARE(Q(Capability::IcmpPing, Env(HostOs::Windows, PackageMode::NativeOrPortable, DisplaySession::NotApplicable)).support,
                 Support::Supported);
        QCOMPARE(Q(Capability::IcmpPing, Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable)).support,
                 Support::Supported);
    }

    void appImageSingleCoreUnsupported() {
        QCOMPARE(Q(Capability::TunSingleCore, Env(HostOs::Linux, PackageMode::AppImage, DisplaySession::X11)).support,
                 Support::Unsupported);
    }
    void debSingleCoreSupported() {
        QCOMPARE(Q(Capability::TunSingleCore, Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11)).support,
                 Support::Supported);
    }

    void buildFlagsOverrideEverything() {
        for (auto os : kAllOs) {
            auto env = Env(os, PackageMode::NativeOrPortable, DisplaySession::NotApplicable);
            env.hotkeyBackendBuilt = false;
            QCOMPARE(Q(Capability::GlobalHotkeys, env).support, Support::Unsupported);
            env.hotkeyBackendBuilt = true;
            env.qrReaderBuilt = false;
            QCOMPARE(Q(Capability::ScreenQrCapture, env).support, Support::Unsupported);
            QCOMPARE(Q(Capability::QrImageImport, env).support, Support::Unsupported);
        }
    }

    void hotkeyBackendSelection() {
        auto with = [](HostOs os, PackageMode m, DisplaySession s, uint v) {
            auto e = Env(os, m, s);
            e.globalShortcutsPortal = v;
            return SelectHotkeyBackend(e);
        };
        for (uint v : {0u, 1u}) {
            QCOMPARE(with(HostOs::Windows, PackageMode::NativeOrPortable, DisplaySession::NotApplicable, v), HotkeyBackend::Native);
            QCOMPARE(with(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable, v), HotkeyBackend::Native);
            QCOMPARE(with(HostOs::Other, PackageMode::NativeOrPortable, DisplaySession::NotApplicable, v), HotkeyBackend::None);
            QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::X11, v), HotkeyBackend::Native);
        }
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland, 0), HotkeyBackend::None);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland, 1), HotkeyBackend::Portal);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::XWayland, 0), HotkeyBackend::Native);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::XWayland, 1), HotkeyBackend::Portal);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::Unknown, 1), HotkeyBackend::Portal);
        for (auto s : kAllSessions) {
            QCOMPARE(with(HostOs::Linux, PackageMode::Flatpak, s, 0), HotkeyBackend::None);
            QCOMPARE(with(HostOs::Linux, PackageMode::Flatpak, s, 1), HotkeyBackend::Portal);
        }
        for (auto os : kAllOs) {
            auto e = Env(os, PackageMode::NativeOrPortable, DisplaySession::Wayland);
            e.globalShortcutsPortal = 1;
            e.hotkeyBackendBuilt = false;
            QCOMPARE(SelectHotkeyBackend(e), HotkeyBackend::None);
        }
    }

    void screenCaptureBackendSelection() {
        auto with = [](HostOs os, PackageMode m, DisplaySession s, uint v) {
            auto e = Env(os, m, s);
            e.screenshotPortal = v;
            return SelectScreenCaptureBackend(e);
        };
        for (uint v : {0u, 1u}) {
            QCOMPARE(with(HostOs::Windows, PackageMode::NativeOrPortable, DisplaySession::NotApplicable, v), ScreenCaptureBackend::Native);
            QCOMPARE(with(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable, v), ScreenCaptureBackend::Native);
            QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::X11, v), ScreenCaptureBackend::Native);
        }
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland, 0), ScreenCaptureBackend::None);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland, 2), ScreenCaptureBackend::Portal);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::XWayland, 0), ScreenCaptureBackend::Native);
        QCOMPARE(with(HostOs::Linux, PackageMode::Deb, DisplaySession::XWayland, 2), ScreenCaptureBackend::Portal);
        QCOMPARE(with(HostOs::Linux, PackageMode::Flatpak, DisplaySession::X11, 0), ScreenCaptureBackend::None);
        QCOMPARE(with(HostOs::Linux, PackageMode::Flatpak, DisplaySession::X11, 1), ScreenCaptureBackend::Portal);
        auto e = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11);
        e.qrReaderBuilt = false;
        QCOMPARE(SelectScreenCaptureBackend(e), ScreenCaptureBackend::None);
    }

    void hotkeyRowFollowsPortal() {
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland);
        env.globalShortcutsPortal = 1;
        auto s = Q(Capability::GlobalHotkeys, env);
        QCOMPARE(s.support, Support::Degraded);
        QVERIFY(IsUsable(s));
        QVERIFY(s.reason.contains("desktop manages"));
        env.globalShortcutsPortal = 0;
        s = Q(Capability::GlobalHotkeys, env);
        QCOMPARE(s.support, Support::Unsupported);
        QVERIFY(s.reason.contains("GNOME 48"));
        env = Env(HostOs::Linux, PackageMode::Flatpak, DisplaySession::X11);
        s = Q(Capability::GlobalHotkeys, env);
        QCOMPARE(s.support, Support::Unsupported);
        QVERIFY(s.reason.contains("Flatpak"));
        QCOMPARE(Q(Capability::GlobalHotkeys, Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11)).support, Support::Supported);
        auto xw = Q(Capability::GlobalHotkeys, Env(HostOs::Linux, PackageMode::AppImage, DisplaySession::XWayland));
        QCOMPARE(xw.support, Support::Degraded);
        QVERIFY(xw.reason.contains("XWayland"));
    }

    void screenRowFollowsPortal() {
        for (auto mode : {PackageMode::Deb, PackageMode::Flatpak}) {
            auto env = Env(HostOs::Linux, mode, DisplaySession::Wayland);
            env.screenshotPortal = 1;
            QCOMPARE(Q(Capability::ScreenQrCapture, env).support, Support::Supported);
            env.screenshotPortal = 0;
            auto s = Q(Capability::ScreenQrCapture, env);
            QCOMPARE(s.support, Support::Unsupported);
            QVERIFY(s.reason.contains("Screenshot portal"));
        }
    }

    void autostartRowFollowsBackgroundPortal() {
        auto env = Env(HostOs::Linux, PackageMode::Flatpak, DisplaySession::Wayland);
        auto s = Q(Capability::AutoStart, env);
        QCOMPARE(s.support, Support::Unsupported);
        QVERIFY(s.reason.contains("Background portal"));
        env.backgroundPortal = 1;
        s = Q(Capability::AutoStart, env);
        QCOMPARE(s.support, Support::Supported);
        QVERIFY(s.reason.isEmpty());
        QCOMPARE(Q(Capability::AutoStart, Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11)).support, Support::Supported);
    }

    void windowsAndMacAnswersIgnorePortals() {
        for (auto os : {HostOs::Windows, HostOs::MacOS})
            for (auto mode : kAllModes)
                for (auto session : kAllSessions)
                    for (auto c : AllCapabilities()) {
                        auto base = Env(os, mode, session);
                        auto withPortals = base;
                        withPortals.backgroundPortal = withPortals.screenshotPortal = withPortals.globalShortcutsPortal = 1;
                        auto a = Q(c, base), b = Q(c, withPortals);
                        QCOMPARE(a.support, b.support);
                        QCOMPARE(a.reason, b.reason);
                    }
        // The pre-52-09 macOS answers, spelled out.
        auto mac = Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable);
        QCOMPARE(Q(Capability::GlobalHotkeys, mac).support, Support::Supported);
        QCOMPARE(Q(Capability::ScreenQrCapture, mac).support, Support::NeedsPermission);
        QCOMPARE(Q(Capability::AutoStart, mac).support, Support::Supported); // phase 53: Start with system works on macOS through a user LaunchAgent
    }

    void sessionFromEnvironmentTable() {
        QCOMPARE(SessionFromEnvironment(HostOs::Windows, "windows", false), DisplaySession::NotApplicable);
        QCOMPARE(SessionFromEnvironment(HostOs::Windows, "windows", true), DisplaySession::NotApplicable);
        QCOMPARE(SessionFromEnvironment(HostOs::MacOS, "cocoa", false), DisplaySession::NotApplicable);
        QCOMPARE(SessionFromEnvironment(HostOs::MacOS, "cocoa", true), DisplaySession::NotApplicable);
        QCOMPARE(SessionFromEnvironment(HostOs::Linux, "wayland", false), DisplaySession::Wayland);
        QCOMPARE(SessionFromEnvironment(HostOs::Linux, "wayland-egl", false), DisplaySession::Wayland);
        QCOMPARE(SessionFromEnvironment(HostOs::Linux, "xcb", true), DisplaySession::XWayland);
        QCOMPARE(SessionFromEnvironment(HostOs::Linux, "xcb", false), DisplaySession::X11);
        QCOMPARE(SessionFromEnvironment(HostOs::Linux, "offscreen", false), DisplaySession::Unknown);
    }

    void everyNonSupportedAnswerHasAReasonAndSupportedHasNone() {
        for (auto os : kAllOs)
            for (auto mode : kAllModes)
                for (auto session : kAllSessions)
                    for (int flags = 0; flags < 4; ++flags)
                        for (auto c : AllCapabilities()) {
                            auto env = Env(os, mode, session);
                            env.hotkeyBackendBuilt = (flags & 1) == 0;
                            env.qrReaderBuilt = (flags & 2) == 0;
                            auto s = Q(c, env);
                            const QString label = CapabilityName(c) + " " + DescribePlatformEnvironment(env);
                            if (s.support == Support::Supported)
                                QVERIFY2(s.reason.isEmpty(), qPrintable(label));
                            else
                                QVERIFY2(!s.reason.isEmpty(), qPrintable(label));
                            QCOMPARE(IsUsable(s), s.support != Support::Unsupported);
                        }
    }

    void compiledHostOsMatchesThisRunner() {
#if defined(Q_OS_WIN)
        QCOMPARE(CompiledHostOs(), HostOs::Windows);
#elif defined(Q_OS_MACOS)
        QCOMPARE(CompiledHostOs(), HostOs::MacOS);
#elif defined(Q_OS_LINUX)
        QCOMPARE(CompiledHostOs(), HostOs::Linux);
#else
        QCOMPARE(CompiledHostOs(), HostOs::Other);
#endif
    }

    void describeIsStable() {
        QCOMPARE(DescribePlatformEnvironment(Env(HostOs::Linux, PackageMode::Flatpak, DisplaySession::Wayland)),
                 QString("os=linux package=flatpak session=wayland tray=yes desktop=unknown portals=background:0,screenshot:0,shortcuts:0"));
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11);
        env.trayAvailable = false;
        env.desktop = LinuxDesktopFamily::Gnome;
        QCOMPARE(DescribePlatformEnvironment(env), QString("os=linux package=deb session=x11 tray=no desktop=gnome portals=background:0,screenshot:0,shortcuts:0"));
        env.backgroundPortal = 1;
        env.screenshotPortal = 2;
        QVERIFY(DescribePlatformEnvironment(env).endsWith("portals=background:1,screenshot:2,shortcuts:0"));
    }

    void systemTrayRowWindowsMacAlwaysSupported() {
        for (auto os : {HostOs::Windows, HostOs::MacOS})
            for (auto mode : kAllModes) {
                auto env = Env(os, mode, DisplaySession::NotApplicable);
                env.trayAvailable = false;
                QCOMPARE(Q(Capability::SystemTray, env).support, Support::Supported);
            }
        QCOMPARE(Q(Capability::SystemTray, Env(HostOs::Other, PackageMode::NativeOrPortable, DisplaySession::NotApplicable)).support,
                 Support::Unsupported);
    }

    void systemTrayRowLinuxWithTrayIsSupported() {
        for (auto mode : kAllModes)
            for (auto session : kAllSessions) {
                auto env = Env(HostOs::Linux, mode, session);
                env.trayAvailable = true;
                QCOMPARE(Q(Capability::SystemTray, env).support, Support::Supported);
            }
    }

    void systemTrayRowLinuxGnomeNoTrayMentionsAppIndicator() {
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland);
        env.trayAvailable = false;
        env.desktop = LinuxDesktopFamily::Gnome;
        const auto s = Q(Capability::SystemTray, env);
        QCOMPARE(s.support, Support::Unsupported);
        QVERIFY(s.reason.contains("AppIndicator"));
    }

    void systemTrayRowLinuxFlatpakNoTrayMentionsFlatpak() {
        for (auto d : {LinuxDesktopFamily::Gnome, LinuxDesktopFamily::Kde, LinuxDesktopFamily::Unknown}) {
            auto env = Env(HostOs::Linux, PackageMode::Flatpak, DisplaySession::Wayland);
            env.trayAvailable = false;
            env.desktop = d;
            const auto s = Q(Capability::SystemTray, env);
            QCOMPARE(s.support, Support::Unsupported);
            QVERIFY(s.reason.contains("Flatpak"));
        }
    }

    void systemTrayRowLinuxOtherDesktopsGenericReason() {
        for (auto d : {LinuxDesktopFamily::Kde, LinuxDesktopFamily::Xfce, LinuxDesktopFamily::Other, LinuxDesktopFamily::Unknown}) {
            auto env = Env(HostOs::Linux, PackageMode::Rpm, DisplaySession::X11);
            env.trayAvailable = false;
            env.desktop = d;
            const auto s = Q(Capability::SystemTray, env);
            QCOMPARE(s.support, Support::Unsupported);
            QVERIFY(s.reason.contains("no system tray"));
            QVERIFY(!s.reason.contains("AppIndicator"));
        }
    }

    void systemTrayIsListedAndNamed() {
        QVERIFY(AllCapabilities().contains(Capability::SystemTray));
        QCOMPARE(CapabilityName(Capability::SystemTray), QString("system-tray"));
    }
    void describeNamesTheCompositorOnLinux() {
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland);
        env.desktop = LinuxDesktopFamily::Other;
        env.compositor = LinuxCompositor::Hyprland;
        QCOMPARE(DescribePlatformEnvironment(env),
                 QString("os=linux package=deb session=wayland tray=yes desktop=other compositor=hyprland portals=background:0,screenshot:0,shortcuts:0"));
        env.desktop = LinuxDesktopFamily::Unknown;
        env.compositor = LinuxCompositor::Sway;
        QVERIFY(DescribePlatformEnvironment(env).contains("desktop=unknown compositor=sway portals="));
    }

    void describeOmitsTheCompositorWhenNoneOrNotLinux() {
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::Wayland);
        env.compositor = LinuxCompositor::None;
        QVERIFY(!DescribePlatformEnvironment(env).contains("compositor="));
        for (auto os : {HostOs::Windows, HostOs::MacOS, HostOs::Other}) {
            auto e = Env(os, PackageMode::NativeOrPortable, DisplaySession::NotApplicable);
            const auto without = DescribePlatformEnvironment(e);
            e.compositor = LinuxCompositor::Hyprland;
            QVERIFY(!DescribePlatformEnvironment(e).contains("compositor="));
            QCOMPARE(DescribePlatformEnvironment(e), without);
        }
    }

    void compositorNeverChangesACapabilityAnswer() {
        const LinuxCompositor all[] = {LinuxCompositor::Hyprland, LinuxCompositor::Sway, LinuxCompositor::Niri,
                                       LinuxCompositor::River, LinuxCompositor::Wayfire, LinuxCompositor::Labwc};
        for (auto os : {HostOs::Windows, HostOs::Linux, HostOs::MacOS, HostOs::Other})
            for (auto mode : kAllModes)
                for (auto session : kAllSessions)
                    for (bool tray : {true, false})
                        for (uint portal : {0u, 1u})
                            for (auto comp : all) {
                                auto base = Env(os, mode, session);
                                base.trayAvailable = tray;
                                base.backgroundPortal = base.screenshotPortal = base.globalShortcutsPortal = portal;
                                auto with = base;
                                with.compositor = comp;
                                for (auto c : AllCapabilities()) {
                                    const auto a = QueryCapability(c, base);
                                    const auto b = QueryCapability(c, with);
                                    QCOMPARE(b.support, a.support);
                                    QCOMPARE(b.reason, a.reason);
                                }
                                QCOMPARE(SelectHotkeyBackend(with), SelectHotkeyBackend(base));
                                QCOMPARE(SelectScreenCaptureBackend(with), SelectScreenCaptureBackend(base));
                            }
    }
};

QTEST_APPLESS_MAIN(PlatformCapabilitiesTest)
#include "platform_capabilities_test.moc"
