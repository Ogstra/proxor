#pragma once
#include "platform/WifiSsid.hpp"
#include "sys/wifi/WifiBackend.hpp"

#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <memory>

// Reads the Wi-Fi network on a worker thread (never on the UI thread) and reports every outcome.
// Qt Core only, so it is unit-tested with fake backends on every CI runner.
class WifiMonitor : public QObject {
    Q_OBJECT
public:
    WifiMonitor(std::unique_ptr<WifiBackend> backend, int intervalMs, QObject *parent = nullptr);
    ~WifiMonitor() override;                  // stops polling, quits the worker, waits at most 5 s

    void setActive(bool active);              // true: read now, then every intervalMs; false: stop the timer
    bool isActive() const;
    void refreshNow();                        // one read even when inactive (On-Demand tab); coalesced with an in-flight read

    ProxorWifi::WifiReading lastReading() const;
    bool hasReading() const;                  // false until the first read completed
    QString currentSsid() const;

    static QString cachedSsid();              // mutex-guarded, safe from any thread
    static WifiMonitor *appInstance();        // the app's monitor (nullptr in tests unless set)
    static void setAppInstance(WifiMonitor *monitor);

signals:
    void ssidChanged(const QString &ssid);                        // cachedSsid() already updated when emitted
    void readingChanged(const ProxorWifi::WifiReading &reading);  // state/ssid/detail/source changed

private:
    void requestRead();
    void onReadingReady(const ProxorWifi::WifiReading &reading);

    std::unique_ptr<WifiBackend> backend_;
    QThread *worker_ = nullptr;
    QObject *reader_ = nullptr;  // lives on worker_
    QTimer *timer_ = nullptr;
    ProxorWifi::ChangeTracker tracker_;
    bool active_ = false;
    bool inFlight_ = false;
    bool again_ = false;
    bool hasReading_ = false;
};
