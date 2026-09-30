#pragma once

// QR decoding from a QImage through ZXing. Compiled to a stub returning {} under NKR_NO_ZXING.

#include <QString>

class QImage;

namespace ProxorPlatform {

QString DecodeQrFromImage(const QImage &image);

} // namespace ProxorPlatform
