#ifdef Q_OS_WIN
#include "WifiMonitor.hpp"
#include <QProcess>
#include <QStringList>

WifiMonitor::WifiMonitor(QObject *parent) : QObject(parent) {
    m_timer = new QTimer(this);
    m_timer->setInterval(5000);
    connect(m_timer, &QTimer::timeout, this, &WifiMonitor::poll);
}

void WifiMonitor::start() {
    poll();           // immediate first check
    m_timer->start();
}

void WifiMonitor::stop() {
    m_timer->stop();
}

void WifiMonitor::poll() {
    auto ssid = currentSsid();
    if (ssid != m_lastSsid) {
        m_lastSsid = ssid;
        emit ssidChanged(ssid);
    }
}

QString WifiMonitor::currentSsid() {
    QProcess proc;
    proc.start("netsh", {"wlan", "show", "interfaces"});
    if (!proc.waitForFinished(3000)) {
        proc.kill();
        return QString();
    }
    auto output = QString::fromLocal8Bit(proc.readAllStandardOutput());
    for (const auto &line : output.split('\n')) {
        auto trimmed = line.trimmed();
        // Match "SSID" but not "BSSID"
        if (trimmed.startsWith("SSID") && !trimmed.startsWith("BSSID")) {
            auto idx = trimmed.indexOf(':');
            if (idx >= 0) {
                return trimmed.mid(idx + 1).trimmed();
            }
        }
    }
    return QString();  // no WiFi or not connected
}
#endif // Q_OS_WIN
