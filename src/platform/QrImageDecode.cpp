#include "platform/QrImageDecode.hpp"

#include <QImage>

#ifndef NKR_NO_ZXING
#include "3rdparty/ZxingQtReader.hpp"
#endif

namespace ProxorPlatform {

#ifndef NKR_NO_ZXING
QString DecodeQrFromImage(const QImage &image) {
    using namespace ZXingQt;
    if (image.isNull()) return {};
    // (a) the screen-scan hints: fast, good for crisp captures
    auto screenHints = DecodeHints().setFormats(BarcodeFormat::QRCode).setTryRotate(false).setBinarizer(Binarizer::FixedThreshold);
    auto text = ReadBarcode(image, screenHints).text();
    if (!text.isEmpty()) return text;
    // (b) local-average binarizer, better for photos and scaled images
    auto photoHints = DecodeHints().setFormats(BarcodeFormat::QRCode).setTryHarder(true).setTryRotate(true);
    return ReadBarcode(image, photoHints).text();
}
#else
QString DecodeQrFromImage(const QImage &) { return {}; }
#endif

} // namespace ProxorPlatform
