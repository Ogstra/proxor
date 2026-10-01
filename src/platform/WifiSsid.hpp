#pragma once
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace ProxorWifi {

enum class ReadState { Connected, NotConnected, PermissionNeeded, Unavailable };

struct WifiReading {
    ReadState state = ReadState::Unavailable;
    QString ssid;    // Connected only
    QString detail;  // user-facing reason for NotConnected/PermissionNeeded/Unavailable (may be empty)
    QString source;  // "netsh", "NetworkManager", "nmcli", "CoreWLAN", "none", "test"...
};
bool operator==(const WifiReading &a, const WifiReading &b);
inline bool operator!=(const WifiReading &a, const WifiReading &b) { return !(a == b); }

enum class PermissionState { NotRequired, NotDetermined, Granted, Denied, Restricted, ServicesDisabled };
enum class PermissionPrompt { None, ExplainThenRequest, PointToSettings };

WifiReading Connected(const QString &ssid, const QString &source);
WifiReading NotConnected(const QString &detail, const QString &source);
WifiReading PermissionNeeded(const QString &detail, const QString &source);
WifiReading Unavailable(const QString &detail, const QString &source);

WifiReading ParseNetshInterfaces(const QString &output);        // Windows `netsh wlan show interfaces`
WifiReading ParseNmcliWifiList(const QByteArray &terseOutput);  // `LC_ALL=C nmcli -t -f ACTIVE,SSID device wifi list --rescan no`
QString DecodeSsidBytes(const QByteArray &raw);                 // UTF-8 when valid, else Latin-1; trailing NULs dropped
bool MonitoringNeeded(bool onDemandEnabled, const QStringList &triggerSsids, const QString &hostsMapping);
bool HostsSkipDiffers(const QString &hostsMapping, const QString &oldSsid, const QString &newSsid);
QString DescribeReading(const WifiReading &reading);            // one line for the On-Demand tab and the log
QString DescribePermission(PermissionState state);              // empty for NotRequired/Granted
PermissionPrompt DecidePermissionPrompt(PermissionState state, bool monitoringNeeded, bool askedThisSession);

class ChangeTracker {
public:
    struct Update {
        bool ssidChanged = false;
        bool readingChanged = false;
        QString ssid;
    };
    Update apply(const WifiReading &reading);
    QString ssid() const;      // current effective SSID ("" = none/unknown)
    WifiReading last() const;  // last applied reading
private:
    QString ssid_;
    WifiReading last_;
    bool hasLast_ = false;
};

} // namespace ProxorWifi
Q_DECLARE_METATYPE(ProxorWifi::WifiReading)
