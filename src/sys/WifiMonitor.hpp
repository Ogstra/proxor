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

signals:
    void ssidChanged(const QString &ssid);  // emitted when SSID changes (including to "")

private slots:
    void poll();

private:
    QTimer *m_timer;
    QString m_lastSsid = "";

    static QString currentSsid();   // runs netsh, returns SSID or "" if none
};
