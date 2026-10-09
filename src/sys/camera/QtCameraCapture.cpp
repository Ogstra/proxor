#include "sys/camera/QtCameraCapture.h"

#include <QCamera>
#include <QDateTime>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QVideoFrame>
#include <QVideoSink>

namespace ProxorQt {

CameraCapture::CameraCapture(QObject *parent) : QObject(parent) {}

CameraCapture::~CameraCapture() { stop(); }

void CameraCapture::start() {
    if (camera) return;
    const auto device = QMediaDevices::defaultVideoInput();
    if (device.isNull()) {
        emit failed(QStringLiteral("No camera found"));
        return;
    }
    camera = new QCamera(device, this);
    session = new QMediaCaptureSession(this);
    sink = new QVideoSink(this);
    session->setCamera(camera);
    session->setVideoSink(sink);
    connect(camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &message) {
        emit failed(message.isEmpty() ? QStringLiteral("Camera unavailable") : message);
    });
    connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame &videoFrame) {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        if (now - lastFrameMs < 100 || !videoFrame.isValid()) return; // about ten frames a second is plenty
        lastFrameMs = now;
        const QImage image = videoFrame.toImage();
        if (!image.isNull()) emit frame(image);
    });
    camera->start();
}

void CameraCapture::stop() {
    if (!camera) return;
    camera->stop();
    delete camera;
    delete session;
    delete sink;
    camera = nullptr;
    session = nullptr;
    sink = nullptr;
}

} // namespace ProxorQt
