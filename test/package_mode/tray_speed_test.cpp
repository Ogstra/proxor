#include <QtTest>

#include "platform/TraySpeed.hpp"

using namespace ProxorPlatform;

class TraySpeedTest : public QObject {
    Q_OBJECT
private slots:
    void rateUnits() {
        // No bytes: the smallest unit is K.
        QCOMPARE(FormatTrayRate(0), QStringLiteral("0.0K/s"));
        QCOMPARE(FormatTrayRate(500), QStringLiteral("0.5K/s"));
        QCOMPARE(FormatTrayRate(999), QStringLiteral("1.0K/s"));
        QCOMPARE(FormatTrayRate(1000), QStringLiteral("1.0K/s"));
        QCOMPARE(FormatTrayRate(12000), QStringLiteral("12.0K/s"));
        QCOMPARE(FormatTrayRate(999949), QStringLiteral("999.9K/s"));
        QCOMPARE(FormatTrayRate(1000000), QStringLiteral("1.0M/s"));
        QCOMPARE(FormatTrayRate(1234567), QStringLiteral("1.2M/s"));
        QCOMPARE(FormatTrayRate(3400000000LL), QStringLiteral("3.4G/s"));
    }
    void negativeIsZero() {
        QCOMPARE(FormatTrayRate(-5), QStringLiteral("0.0K/s"));
    }
    void twoLines() {
        QCOMPARE(FormatTraySpeed(12000, 16000), QStringLiteral("12.0K/s↑\n16.0K/s↓"));
    }
};

QTEST_MAIN(TraySpeedTest)
#include "tray_speed_test.moc"
