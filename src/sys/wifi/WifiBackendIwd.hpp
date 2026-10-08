#pragma once
#include "sys/wifi/WifiBackend.hpp"
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QMap>
#include <QString>
#include <QVariantMap>

using IwdInterfaces = QMap<QString, QVariantMap>;              // interface -> properties
using IwdManagedObjects = QMap<QDBusObjectPath, IwdInterfaces>; // a{oa{sa{sv}}}
Q_DECLARE_METATYPE(IwdInterfaces)
Q_DECLARE_METATYPE(IwdManagedObjects)

enum class IwdPresence { Present, Unreachable, Denied, TimedOut, Failed, Sandboxed };

// Reads the connected Wi-Fi name from iwd (net.connman.iwd) over D-Bus. QtCore + QtDBus only.
class IwdWifiReader final : public WifiBackend {
public:
    IwdWifiReader(QDBusConnection bus, bool sandboxed, int budgetMs = 1500);
    ProxorWifi::WifiReading read() override;             // readWithin(budgetMs)
    ProxorWifi::WifiReading readWithin(int budgetMs);    // one GetManagedObjects call bounded by budgetMs (min 200)
    IwdPresence presence() const;                        // how the last read went
private:
    QDBusConnection bus_;
    bool sandboxed_;
    int budgetMs_;
    IwdPresence presence_ = IwdPresence::Unreachable;
};
