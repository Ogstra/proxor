#pragma once

// Compact traffic rate text for the macOS menu bar (Qt Core only).

#include <QString>

namespace ProxorPlatform {

// "0.5K/s", "12.0K/s", "1.2M/s", "3.4G/s": never bytes, the smallest unit is K. Decimal units like ReadableSize.
// Negative counts as 0.
QString FormatTrayRate(qint64 bytesPerSecond);

// Two lines: "<up>↑\n<down>↓".
QString FormatTraySpeed(qint64 uploadBytesPerSecond, qint64 downloadBytesPerSecond);

} // namespace ProxorPlatform
