#include "platform/PlatformCapabilitiesApp.hpp"

#include "main/ProxorGui.hpp"

#include "platform/LinuxDesktop.hpp"

#include <QGuiApplication>
#include <QSystemTrayIcon>

namespace ProxorPlatform {

PlatformEnvironment CurrentPlatformEnvironment() {
    PlatformEnvironment env;
    env.os = CompiledHostOs();
    env.packageMode = ProxorGui::CurrentPackageMode();
    env.session = SessionFromEnvironment(env.os, QGuiApplication::platformName(),
                                         !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"));
    // Fresh on every call: a tray host can appear later (right after login).
    env.trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (env.os == HostOs::Linux) env.desktop = DetectLinuxDesktop(LinuxDesktopEnvFromProcess()).family;
#ifdef NKR_NO_QHOTKEY
    env.hotkeyBackendBuilt = false;
#endif
#ifdef NKR_NO_ZXING
    env.qrReaderBuilt = false;
#endif
    return env;
}

CapabilityStatus CurrentCapability(Capability capability) {
    return QueryCapability(capability, CurrentPlatformEnvironment());
}

} // namespace ProxorPlatform
