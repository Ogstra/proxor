#include "platform/TraySpeed.hpp"

namespace ProxorPlatform {

QString FormatTrayRate(qint64 bytesPerSecond) {
    const double value = bytesPerSecond < 0 ? 0.0 : static_cast<double>(bytesPerSecond);
    if (value < 1000.0 * 1000.0) return QString::number(value / 1000.0, 'f', 1) + QStringLiteral("K/s");
    if (value < 1000.0 * 1000.0 * 1000.0) return QString::number(value / 1e6, 'f', 1) + QStringLiteral("M/s");
    return QString::number(value / 1e9, 'f', 1) + QStringLiteral("G/s");
}

QString FormatTraySpeed(qint64 uploadBytesPerSecond, qint64 downloadBytesPerSecond) {
    return FormatTrayRate(uploadBytesPerSecond) + QStringLiteral("↑\n") + FormatTrayRate(downloadBytesPerSecond) +
           QStringLiteral("↓");
}

} // namespace ProxorPlatform
