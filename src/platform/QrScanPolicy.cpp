#include "platform/QrScanPolicy.hpp"

#include <QCoreApplication>

namespace ProxorPlatform {

namespace {
QString T(const char *s) { return QCoreApplication::translate("QrScan", s); }
} // namespace

QString QrScanMessage(QrSource source, const QrScanResult &result, const CapabilityStatus &screenCapture) {
    if (result.decoded) return {};
    switch (source) {
        case QrSource::Screen: {
            if (!result.imageAvailable) {
                return T("Proxor could not capture the screen here. %1 Use Add from QR Code in Image File or Add from QR Code in Clipboard Image instead.")
                    .arg(screenCapture.reason)
                    .simplified();
            }
            QString msg = T("QR Code not found");
            if (screenCapture.support == Support::NeedsPermission || screenCapture.support == Support::Degraded) {
                if (!screenCapture.reason.isEmpty()) msg += QStringLiteral(". ") + screenCapture.reason;
            } else {
                msg += QStringLiteral(". ") + T("You can also use Add from QR Code in Image File.");
            }
            return msg;
        }
        case QrSource::ImageFile:
            if (!result.imageAvailable) return T("The selected file could not be read as an image.");
            return T("No QR code found in this image");
        case QrSource::ClipboardImage:
            if (!result.imageAvailable) return T("The clipboard does not contain an image");
            return T("No QR code found in the clipboard image");
    }
    return {};
}

MacScreenScanDecision DecideMacScreenScan(bool preflightGranted, bool requestedThisSession) {
    MacScreenScanDecision d;
    if (preflightGranted) d.capture = true;
    else if (!requestedThisSession) d.requestAccess = true;
    return d;
}

QString MacScreenRecordingMessage() {
    return T("Proxor needs the Screen Recording permission to look for a QR code on the screen. "
             "Allow Proxor in System Settings > Privacy & Security > Screen & System Audio Recording, "
             "then quit and reopen Proxor (macOS applies the permission after a restart). "
             "If Proxor is already listed and turned on, turn it off and on again: macOS ties the permission "
             "to the app's signature, which changes with every Proxor update (brew upgrade). "
             "Adding the QR code from an image file or the clipboard needs no permission.");
}

} // namespace ProxorPlatform
