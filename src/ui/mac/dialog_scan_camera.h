#pragma once

// macOS: live camera preview that closes as soon as a QR code is read. Listed only in cmake/macos/macos.cmake.

#include <QDialog>
#include <QString>

class QLabel;
class QImage;

namespace ProxorMac { class CameraCapture; }

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
    ProxorMac::CameraCapture *camera;
    qint64 lastDecodeMs = 0;
    QString decoded;
    QString failureReason;
};
