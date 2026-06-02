#pragma once
#include <QObject>
#include <QString>
#include <QTimer>

class WifiMonitor : public QObject {
    Q_OBJECT
public:
    explicit WifiMonitor(QObject *parent = nullptr);

    void start();   // begin polling every 5000 ms
    void stop();    // stop the timer

    // Last polled SSID (cached). Returns "" if no WiFi connected or not yet polled.
    // Safe to call from any thread that only reads; cache is updated by poll() on UI thread.
    static QString cachedSsid();

signals:
    void ssidChanged(const QString &ssid);  // emitted when SSID changes (including to "")

private slots:
    void poll();

private:
    QTimer *m_timer;
    QString m_lastSsid = "";

    static QString currentSsid();   // runs netsh, returns SSID or "" if none
    static QString s_cachedSsid;
};
