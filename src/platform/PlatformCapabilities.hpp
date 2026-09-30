#pragma once

// Single truth table for platform-dependent features: what works, what is degraded, what needs an OS
// permission and what is unavailable, with a user-facing reason for every non-Supported answer.
// Qt Core only (no QtGui/QtWidgets, no app globals) so it is unit-tested on every CI runner.

#include "main/PackageMode.hpp"

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
};

enum class Capability {
    GlobalHotkeys,
    ScreenQrCapture,
    QrImageImport,
    IcmpPing,
    AutoStart,
    OnDemandSsid,
    TunMode,
    TunStrictRoute,
    TunSingleCore
};

enum class Support { Supported, Degraded, NeedsPermission, Unsupported };

struct CapabilityStatus {
    Support support = Support::Supported;
    QString reason;
};

HostOs CompiledHostOs();
DisplaySession SessionFromEnvironment(HostOs os, const QString &qpaPlatformName, bool waylandDisplaySet);
CapabilityStatus QueryCapability(Capability capability, const PlatformEnvironment &env);
bool IsUsable(const CapabilityStatus &status);
QString CapabilityName(Capability capability);
QString DescribePlatformEnvironment(const PlatformEnvironment &env);
QList<Capability> AllCapabilities();

} // namespace ProxorPlatform
