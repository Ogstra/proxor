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
    // failed() can fire from inside start() (no camera, access denied), before exec() runs the event loop.
    // A reject() issued that early is lost and the dialog would stay open on a black preview, so queue it.
    connect(camera, &Capture::failed, this, [this](const QString &reason) {
        if (failureReason.isEmpty()) failureReason = reason;
        QMetaObject::invokeMethod(this, &QDialog::reject, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
    camera->start();
}

DialogScanCamera::~DialogScanCamera() { camera->stop(); }

// accept(), reject(), Esc and the window close button all end here: release the camera on every path.
void DialogScanCamera::done(int result) {
    camera->stop();
    QDialog::done(result);
}

void DialogScanCamera::onFrame(const QImage &image) {
    if (!decoded.isEmpty()) return; // a frame queued before stop() must not accept twice
    preview->setPixmap(QPixmap::fromImage(image.mirrored(true, false))
                           .scaled(preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastDecodeMs < 150) return;
    lastDecodeMs = now;
    const auto text = ProxorPlatform::DecodeQrFromImage(image);
    if (text.isEmpty()) return;
    decoded = text;
    accept();
}
