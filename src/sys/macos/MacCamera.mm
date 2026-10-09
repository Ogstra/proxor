// macOS only (listed only in cmake/macos/macos.cmake, compiled with -fobjc-arc).
#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include "sys/macos/MacCamera.h"

#include <QMetaObject>
#include <QPointer>
#include <atomic>
#include <functional>

@interface ProxorCameraSink : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, assign) std::atomic<bool> *pending;
@property(nonatomic, copy) void (^deliver)(QImage);
@end

@implementation ProxorCameraSink
- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
    if (self.pending->exchange(true)) return; // the GUI thread has not taken the last frame yet
    CVImageBufferRef buffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!buffer) { self.pending->store(false); return; }
    CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    const QImage view(static_cast<const uchar *>(CVPixelBufferGetBaseAddress(buffer)),
                      int(CVPixelBufferGetWidth(buffer)), int(CVPixelBufferGetHeight(buffer)),
                      int(CVPixelBufferGetBytesPerRow(buffer)), QImage::Format_ARGB32);
    const QImage copy = view.copy(); // BGRA in memory is ARGB32 on little-endian; copy before unlocking
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    self.deliver(copy);
}
@end

namespace ProxorMac {

struct CameraCapture::Impl {
    AVCaptureSession *session = nil;
    ProxorCameraSink *sink = nil;
    dispatch_queue_t queue = nullptr;
    std::atomic<bool> pending{false};
};

CameraCapture::CameraCapture(QObject *parent) : QObject(parent), d(new Impl) {}

CameraCapture::~CameraCapture() { stop(); }

void CameraCapture::start() {
    if (d->session) return;
    QPointer<CameraCapture> self(this);
    std::function<void()> begin = [self] {
        CameraCapture *c = self.data();
        if (!c || c->d->session) return;
        AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        if (!device) { emit c->failed(QStringLiteral("No camera found")); return; }
        NSError *err = nil;
        AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&err];
        if (!input) { emit c->failed(QString::fromNSString(err.localizedDescription ?: @"Camera unavailable")); return; }
        AVCaptureSession *session = [[AVCaptureSession alloc] init];
        if ([session canSetSessionPreset:AVCaptureSessionPreset640x480]) session.sessionPreset = AVCaptureSessionPreset640x480;
        AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
        output.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32BGRA)};
        output.alwaysDiscardsLateVideoFrames = YES;
        if (![session canAddInput:input] || ![session canAddOutput:output]) {
            emit c->failed(QStringLiteral("Camera unavailable"));
            return;
        }
        [session addInput:input];
        [session addOutput:output];
        ProxorCameraSink *sink = [[ProxorCameraSink alloc] init];
        sink.pending = &c->d->pending;
        sink.deliver = ^(QImage image) {
            QMetaObject::invokeMethod(
                self.data(),
                [self, image] {
                    if (!self) return;
                    self->d->pending.store(false);
                    emit self->frame(image);
                },
                Qt::QueuedConnection);
        };
        c->d->queue = dispatch_queue_create("proxor.camera", DISPATCH_QUEUE_SERIAL);
        [output setSampleBufferDelegate:sink queue:c->d->queue];
        c->d->sink = sink;
        c->d->session = session;
        dispatch_async(c->d->queue, ^{ [session startRunning]; });
    };

    switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
    case AVAuthorizationStatusAuthorized:
        begin();
        break;
    case AVAuthorizationStatusNotDetermined: {
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                 completionHandler:^(BOOL granted) {
            QMetaObject::invokeMethod(
                self.data(),
                [self, granted, begin] {
                    if (!self) return;
                    if (granted) begin();
                    else emit self->failed(QStringLiteral("Camera access was not allowed"));
                },
                Qt::QueuedConnection);
        }];
        break;
    }
    default:
        emit failed(QStringLiteral("Camera access was not allowed"));
        break;
    }
}

void CameraCapture::stop() {
    if (!d->session) return;
    AVCaptureSession *session = d->session;
    d->sink.deliver = ^(QImage) {};
    d->session = nil;
    d->sink = nil;
    if (d->queue) dispatch_async(d->queue, ^{ [session stopRunning]; });
}

} // namespace ProxorMac
