#pragma once

// macOS camera frames for the QR scan. Listed only in cmake/macos/macos.cmake; the OS shows its own permission prompt.

#include <QImage>
#include <QObject>
#include <memory>

namespace ProxorMac {

class CameraCapture : public QObject {
    Q_OBJECT
public:
    explicit CameraCapture(QObject *parent = nullptr);
    ~CameraCapture() override;

    void start(); // asks macOS for access the first time; reports through frame() or failed()
    void stop();

signals:
    void frame(const QImage &image); // upright, not mirrored; always delivered on this object's thread
    void failed(const QString &reason);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace ProxorMac
