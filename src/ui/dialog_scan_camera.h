#pragma once

// Live camera preview that closes as soon as a QR code is read. Listed in cmake/macos/macos.cmake, and in
// CMakeLists.txt when Qt Multimedia is found (Windows, Linux).

#include <QDialog>
#include <QString>

class QLabel;
class QImage;

namespace ProxorMac { class CameraCapture; }
namespace ProxorQt { class CameraCapture; }

class DialogScanCamera : public QDialog {
    Q_OBJECT
public:
    explicit DialogScanCamera(QWidget *parent = nullptr);
    ~DialogScanCamera() override;

    QString text() const { return decoded; }
    QString failure() const { return failureReason; }

private:
    void onFrame(const QImage &image);

    QLabel *preview;
#ifdef Q_OS_MACOS
    ProxorMac::CameraCapture *camera;
#else
    ProxorQt::CameraCapture *camera;
#endif
    qint64 lastDecodeMs = 0;
    QString decoded;
    QString failureReason;
};
