#pragma once

// Which message (if any) the user gets after a QR import attempt. Qt Core only, unit-tested.

#include "platform/PlatformCapabilities.hpp"

#include <QString>

namespace ProxorPlatform {

enum class QrSource { Screen, ImageFile, ClipboardImage };

struct QrScanResult {
    bool imageAvailable = false;
    bool decoded = false;
};

// Empty when decoded; otherwise the user-facing message for this source/result/capability.
QString QrScanMessage(QrSource source, const QrScanResult &result, const CapabilityStatus &screenCapture);

// macOS Screen Recording gate (phase 53). granted -> capture; not granted and not yet requested this
// session -> request access; otherwise only explain.
struct MacScreenScanDecision {
    bool capture = false;
    bool requestAccess = false;
};
MacScreenScanDecision DecideMacScreenScan(bool preflightGranted, bool requestedThisSession);

} // namespace ProxorPlatform
