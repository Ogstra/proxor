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
        for (auto c : {Capability::GlobalHotkeys, Capability::ScreenQrCapture}) {
            auto s = Q(c, env);
            QCOMPARE(s.support, Support::Unsupported);
            QVERIFY(s.reason.contains("Wayland"));
        }
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
    void macosAutostartUnsupported() {
        auto s = Q(Capability::AutoStart, Env(HostOs::MacOS, PackageMode::NativeOrPortable, DisplaySession::NotApplicable));
        QCOMPARE(s.support, Support::Unsupported);
        QVERIFY(s.reason.contains("Login Items"));
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
                 QString("os=linux package=flatpak session=wayland tray=yes desktop=unknown"));
        auto env = Env(HostOs::Linux, PackageMode::Deb, DisplaySession::X11);
        env.trayAvailable = false;
        env.desktop = LinuxDesktopFamily::Gnome;
        QCOMPARE(DescribePlatformEnvironment(env), QString("os=linux package=deb session=x11 tray=no desktop=gnome"));
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
};

QTEST_APPLESS_MAIN(PlatformCapabilitiesTest)
#include "platform_capabilities_test.moc"
