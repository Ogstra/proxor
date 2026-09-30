#include <QtTest>
#include <QImage>
#include <QTransform>

#include "platform/QrImageDecode.hpp"
#include "3rdparty/qrcodegen.hpp"

using namespace ProxorPlatform;

static QImage MakeQr(const QString &text) {
    const auto qr = qrcodegen::QrCode::encodeText(text.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 4, scale = 8, n = qr.getSize();
    QImage img((n + 2 * border) * scale, (n + 2 * border) * scale, QImage::Format_RGB32);
    img.fill(Qt::white);
    for (int y = 0; y < n; y++)
        for (int x = 0; x < n; x++)
            if (qr.getModule(x, y))
                for (int dy = 0; dy < scale; dy++)
                    for (int dx = 0; dx < scale; dx++)
                        img.setPixel((x + border) * scale + dx, (y + border) * scale + dy, qRgb(0, 0, 0));
    return img;
}

class QrImageDecodeTest : public QObject {
    Q_OBJECT
private slots:
    void upright() {
        const QString link = "vless://uuid@example.com:443#test";
        QCOMPARE(DecodeQrFromImage(MakeQr(link)), link);
    }
    void rotated() {
        const QString link = "vless://uuid@example.com:443#test";
        QCOMPARE(DecodeQrFromImage(MakeQr(link).transformed(QTransform().rotate(90))), link);
    }
    void blank() {
        QImage img(200, 200, QImage::Format_RGB32);
        img.fill(Qt::white);
        QVERIFY(DecodeQrFromImage(img).isEmpty());
    }
    void nullImage() { QVERIFY(DecodeQrFromImage(QImage()).isEmpty()); }
};

QTEST_MAIN(QrImageDecodeTest)
#include "qr_image_decode_test.moc"
