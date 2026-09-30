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

} // namespace ProxorPlatform
