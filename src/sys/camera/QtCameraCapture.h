#pragma once

// Camera frames through Qt Multimedia, for the QR scan on Windows and Linux. Listed in CMakeLists.txt
// only when Qt Multimedia is found; macOS uses MacCamera instead.

#include <QImage>
#include <QObject>

class QCamera;
class QMediaCaptureSession;
class QVideoSink;

namespace ProxorQt {

class CameraCapture : public QObject {
    Q_OBJECT
public:
    explicit CameraCapture(QObject *parent = nullptr);
    ~CameraCapture() override;

    void start(); // reports through frame() or failed()
    void stop();

signals:
    void frame(const QImage &image); // upright, not mirrored
    void failed(const QString &reason);

private:
    QCamera *camera = nullptr;
    QMediaCaptureSession *session = nullptr;
    QVideoSink *sink = nullptr;
    qint64 lastFrameMs = 0;
};

} // namespace ProxorQt
