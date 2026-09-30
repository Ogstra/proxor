#include "platform/PlatformCapabilitiesApp.hpp"

#include "main/ProxorGui.hpp"

#include <QGuiApplication>

namespace ProxorPlatform {

PlatformEnvironment CurrentPlatformEnvironment() {
    PlatformEnvironment env;
    env.os = CompiledHostOs();
    env.packageMode = ProxorGui::CurrentPackageMode();
    env.session = SessionFromEnvironment(env.os, QGuiApplication::platformName(),
                                         !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"));
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
