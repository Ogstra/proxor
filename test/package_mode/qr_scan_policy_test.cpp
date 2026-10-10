#include <QtTest>

#include "platform/QrScanPolicy.hpp"

using namespace ProxorPlatform;

class QrScanPolicyTest : public QObject {
    Q_OBJECT
private slots:
    void decodedIsEmpty() {
        for (auto s : {QrSource::Screen, QrSource::ImageFile, QrSource::ClipboardImage})
            QVERIFY(QrScanMessage(s, {true, true}, {Support::Supported, {}}).isEmpty());
    }
    void screenNoImage() {
        const auto m = QrScanMessage(QrSource::Screen, {false, false}, {Support::Supported, {}});
        QVERIFY(m.contains("could not capture the screen"));
        QVERIFY(m.contains("image file", Qt::CaseInsensitive));
        QVERIFY(m.contains("clipboard", Qt::CaseInsensitive));
        QVERIFY(m != "QR Code not found");
    }
    void screenPermission() {
        const auto m = QrScanMessage(QrSource::Screen, {true, false}, {Support::NeedsPermission, "Grant Screen Recording"});
        QVERIFY(m.contains("QR Code not found"));
        QVERIFY(m.contains("Grant Screen Recording"));
    }
    void screenDegraded() {
        QVERIFY(QrScanMessage(QrSource::Screen, {true, false}, {Support::Degraded, "XWayland only"}).contains("XWayland only"));
    }
    void screenSupported() {
        const auto m = QrScanMessage(QrSource::Screen, {true, false}, {Support::Supported, {}});
        QVERIFY(m.contains("QR Code not found"));
        QVERIFY(m.contains("Image File"));
    }
    void imageFile() {
        QVERIFY(QrScanMessage(QrSource::ImageFile, {false, false}, {}).contains("could not be read"));
        QVERIFY(QrScanMessage(QrSource::ImageFile, {true, false}, {}).contains("No QR code found in this image"));
    }
    void clipboard() {
        QVERIFY(QrScanMessage(QrSource::ClipboardImage, {false, false}, {}).contains("The clipboard does not contain an image"));
        QVERIFY(QrScanMessage(QrSource::ClipboardImage, {true, false}, {}).contains("No QR code found in the clipboard image"));
    }
    void macDecision() {
        const auto g = DecideMacScreenScan(true, false);
        QVERIFY(g.capture && !g.requestAccess);
        const auto g2 = DecideMacScreenScan(true, true);
        QVERIFY(g2.capture && !g2.requestAccess);
        const auto first = DecideMacScreenScan(false, false);
        QVERIFY(!first.capture && first.requestAccess);
        const auto again = DecideMacScreenScan(false, true);
        QVERIFY(!again.capture && !again.requestAccess);
    }
    void macGrantedMiss() {
        const auto m = QrScanMessage(QrSource::Screen, {true, false}, {Support::Supported, {}});
        QCOMPARE(m, QString("QR Code not found. You can also use Add from QR Code in Image File."));
        QVERIFY(!m.contains("Screen Recording"));
    }
};

QTEST_APPLESS_MAIN(QrScanPolicyTest)
#include "qr_scan_policy_test.moc"
