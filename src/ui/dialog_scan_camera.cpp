#include "ui/dialog_scan_camera.h"

#include "platform/QrImageDecode.hpp"
#ifdef Q_OS_MACOS
#include "sys/macos/MacCamera.h"
#else
#include "sys/camera/QtCameraCapture.h"
#endif

#include <QDateTime>
#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

DialogScanCamera::DialogScanCamera(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Add from QR Code with Camera"));
    preview = new QLabel(this);
    preview->setFixedSize(480, 360);
    preview->setAlignment(Qt::AlignCenter);
    preview->setStyleSheet("background: black;");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(preview);

#ifdef Q_OS_MACOS
    using Capture = ProxorMac::CameraCapture;
#else
    using Capture = ProxorQt::CameraCapture;
#endif
    camera = new Capture(this);
    connect(camera, &Capture::frame, this, &DialogScanCamera::onFrame);
    connect(camera, &Capture::failed, this, [this](const QString &reason) {
        failureReason = reason;
        reject();
    });
    camera->start();
}

DialogScanCamera::~DialogScanCamera() { camera->stop(); }

void DialogScanCamera::onFrame(const QImage &image) {
    preview->setPixmap(QPixmap::fromImage(image.mirrored(true, false))
                           .scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastDecodeMs < 150) return;
    lastDecodeMs = now;
    const auto text = ProxorPlatform::DecodeQrFromImage(image);
    if (text.isEmpty()) return;
    decoded = text;
    camera->stop();
    accept();
}
