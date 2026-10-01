#include "sys/WifiMonitor.hpp"

#include <QDebug>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex g_mutex;
QString g_cachedSsid;
WifiMonitor *g_app = nullptr;
} // namespace

QString WifiMonitor::cachedSsid() {
    QMutexLocker lock(&g_mutex);
    return g_cachedSsid;
}

WifiMonitor *WifiMonitor::appInstance() {
    QMutexLocker lock(&g_mutex);
    return g_app;
}

void WifiMonitor::setAppInstance(WifiMonitor *monitor) {
    QMutexLocker lock(&g_mutex);
    g_app = monitor;
}

WifiMonitor::WifiMonitor(std::unique_ptr<WifiBackend> backend, int intervalMs, QObject *parent)
    : QObject(parent), backend_(std::move(backend)) {
    qRegisterMetaType<ProxorWifi::WifiReading>();
    worker_ = new QThread;
    worker_->setObjectName(QStringLiteral("WifiMonitor"));
    reader_ = new QObject;
    reader_->moveToThread(worker_);
    worker_->start();

    timer_ = new QTimer(this);
    timer_->setInterval(intervalMs);
    connect(timer_, &QTimer::timeout, this, &WifiMonitor::requestRead);
}

WifiMonitor::~WifiMonitor() {
    timer_->stop();
    if (appInstance() == this) setAppInstance(nullptr);
    worker_->quit();
    if (worker_->wait(5000)) {
        reader_->moveToThread(QThread::currentThread());
        delete reader_;
        delete worker_;
    } else {
        // The backend broke its ~4 s contract. Leak the thread and the backend rather than crash.
        qWarning("WifiMonitor: the Wi-Fi backend did not return in time; leaving its thread running");
        backend_.release();
    }
}

void WifiMonitor::setActive(bool active) {
    if (active == active_) return;
    active_ = active;
    if (active) {
        timer_->start();
        requestRead();
    } else {
        timer_->stop();
    }
}

bool WifiMonitor::isActive() const { return active_; }

void WifiMonitor::refreshNow() { requestRead(); }

ProxorWifi::WifiReading WifiMonitor::lastReading() const { return tracker_.last(); }
bool WifiMonitor::hasReading() const { return hasReading_; }
QString WifiMonitor::currentSsid() const { return tracker_.ssid(); }

void WifiMonitor::requestRead() {
    if (inFlight_) {
        again_ = true;
        return;
    }
    inFlight_ = true;
    QMetaObject::invokeMethod(
        reader_,
        [this, backend = backend_.get()] {
            const ProxorWifi::WifiReading reading = backend->read();
            QMetaObject::invokeMethod(this, [this, reading] { onReadingReady(reading); }, Qt::QueuedConnection);
        },
        Qt::QueuedConnection);
}

void WifiMonitor::onReadingReady(const ProxorWifi::WifiReading &reading) {
    inFlight_ = false;
    hasReading_ = true;
    const auto update = tracker_.apply(reading);
    {
        QMutexLocker lock(&g_mutex);
        g_cachedSsid = tracker_.ssid();
    }
    if (update.readingChanged) emit readingChanged(reading);
    if (update.ssidChanged) emit ssidChanged(update.ssid);
    if (again_) {
        again_ = false;
        requestRead();
    }
}
