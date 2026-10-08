#include "platform/WifiSsid.hpp"

#include <QCoreApplication>
#include <QSet>
#include <QStringDecoder>

namespace ProxorWifi {

namespace {

QString T(const char *text) { return QCoreApplication::translate("WifiSsid", text); }

// Third column ("skip on SSIDs") of every non-comment hosts line, as a set of exact trimmed SSIDs.
// Returns false in `any` when no line carries a usable third column.
QSet<QString> SkipSsids(const QString &hostsMapping) {
    QSet<QString> set;
    const QStringList lines = hostsMapping.split('\n');
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        const QStringList cols = line.simplified().split(' ', Qt::SkipEmptyParts);
        if (cols.size() < 3) continue;
        const QStringList ssids = cols.at(2).split(',');
        for (const QString &s : ssids) {
            const QString t = s.trimmed();
            if (!t.isEmpty()) set.insert(t);
        }
    }
    return set;
}

} // namespace

bool operator==(const WifiReading &a, const WifiReading &b) {
    return a.state == b.state && a.ssid == b.ssid && a.detail == b.detail && a.source == b.source;
}

WifiReading Connected(const QString &ssid, const QString &source) {
    return WifiReading{ReadState::Connected, ssid, QString(), source};
}
WifiReading NotConnected(const QString &detail, const QString &source) {
    return WifiReading{ReadState::NotConnected, QString(), detail, source};
}
WifiReading PermissionNeeded(const QString &detail, const QString &source) {
    return WifiReading{ReadState::PermissionNeeded, QString(), detail, source};
}
WifiReading Unavailable(const QString &detail, const QString &source) {
    return WifiReading{ReadState::Unavailable, QString(), detail, source};
}

WifiReading ParseNetshInterfaces(const QString &output) {
    const QString source = QStringLiteral("netsh");
    bool sawEmptySsid = false;
    const QStringList lines = output.split('\n');
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        // Match "SSID" but not "BSSID"
        if (trimmed.startsWith("SSID") && !trimmed.startsWith("BSSID")) {
            const qsizetype idx = trimmed.indexOf(':');
            if (idx >= 0) {
                const QString ssid = trimmed.mid(idx + 1).trimmed();
                if (!ssid.isEmpty()) return Connected(ssid, source);
                sawEmptySsid = true;
            }
        }
    }
    if (sawEmptySsid) return NotConnected(T("Not connected to a Wi-Fi network."), source);
    if (output.contains("location permission", Qt::CaseInsensitive)) {
        return PermissionNeeded(T("Windows blocks reading the Wi-Fi network name until location access is allowed. "
                                  "Turn it on in Settings > Privacy & security > Location."),
                                source);
    }
    if (output.contains("no wireless interface", Qt::CaseInsensitive)) {
        return NotConnected(T("No Wi-Fi adapter was found on this computer."), source);
    }
    return NotConnected(QString(), source);
}

QString DecodeSsidBytes(const QByteArray &rawIn) {
    QByteArray raw = rawIn;
    while (!raw.isEmpty() && raw.back() == '\0') raw.chop(1);
    if (raw.isEmpty()) return QString();
    QStringDecoder decoder(QStringDecoder::Utf8);
    const QString text = decoder.decode(raw);
    if (decoder.hasError()) return QString::fromLatin1(raw);
    return text;
}

WifiReading ParseNmcliWifiList(const QByteArray &terseOutput) {
    const QString source = QStringLiteral("nmcli");
    const QList<QByteArray> lines = terseOutput.split('\n');
    for (const QByteArray &line : lines) {
        if (line.trimmed().isEmpty()) continue;
        // Split at the first unescaped ':'; unescape '\x' to 'x' in both halves.
        QByteArray active, ssid;
        QByteArray *cur = &active;
        bool split = false;
        for (qsizetype i = 0; i < line.size(); ++i) {
            const char c = line.at(i);
            if (c == '\\' && i + 1 < line.size()) {
                cur->append(line.at(++i));
            } else if (c == ':' && !split) {
                split = true;
                cur = &ssid;
            } else {
                cur->append(c);
            }
        }
        if (!split || active.trimmed() != "yes") continue;
        const QString decoded = DecodeSsidBytes(ssid);
        if (decoded.isEmpty()) {
            return NotConnected(T("Connected to a Wi-Fi network with a hidden name."), source);
        }
        return Connected(decoded, source);
    }
    return NotConnected(QString(), source);
}

bool MonitoringNeeded(bool onDemandEnabled, const QStringList &triggerSsids, const QString &hostsMapping) {
    if (onDemandEnabled) {
        for (const QString &s : triggerSsids) {
            if (!s.trimmed().isEmpty()) return true;
        }
    }
    return !SkipSsids(hostsMapping).isEmpty();
}

bool HostsSkipDiffers(const QString &hostsMapping, const QString &oldSsid, const QString &newSsid) {
    const QSet<QString> set = SkipSsids(hostsMapping);
    if (set.isEmpty()) return false;
    const bool oldSkipped = !oldSsid.isEmpty() && set.contains(oldSsid);
    const bool newSkipped = !newSsid.isEmpty() && set.contains(newSsid);
    return oldSkipped != newSkipped;
}

QString DescribeReading(const WifiReading &r) {
    switch (r.state) {
        case ReadState::Connected: {
            QString text = T("Connected to Wi-Fi network \"%1\".").arg(r.ssid);
            if (!r.source.isEmpty()) text += T(" (via %1)").arg(r.source);
            return text;
        }
        case ReadState::NotConnected:
            return r.detail.isEmpty() ? T("Not connected to a Wi-Fi network.") : r.detail;
        case ReadState::PermissionNeeded:
            return r.detail.isEmpty() ? T("Proxor is not allowed to read the Wi-Fi network name.") : r.detail;
        case ReadState::Unavailable:
            return r.detail.isEmpty() ? T("Wi-Fi network detection is not available.") : r.detail;
    }
    return T("Wi-Fi network detection is not available.");
}

QString DescribePermission(PermissionState state) {
    switch (state) {
        case PermissionState::NotRequired:
        case PermissionState::Granted:
            return QString();
        case PermissionState::NotDetermined:
            return T("The operating system treats Wi-Fi network names as location data, so Proxor must ask for "
                     "Location access to read it. Proxor never reads or stores your location.");
        case PermissionState::Denied:
            return T("Location access for Proxor is off. Turn it on in System Settings > Privacy & Security > "
                     "Location Services so On-Demand and \"Skip on SSIDs\" can see the Wi-Fi network.");
        case PermissionState::Restricted:
            return T("Location access is restricted by a profile or an administrator, so Proxor cannot read the "
                     "Wi-Fi network name.");
        case PermissionState::ServicesDisabled:
            return T("Location Services is turned off. Turn it on in System Settings > Privacy & Security > "
                     "Location Services.");
    }
    return QString();
}

PermissionPrompt DecidePermissionPrompt(PermissionState state, bool monitoringNeeded, bool askedThisSession) {
    if (!monitoringNeeded) return PermissionPrompt::None;
    switch (state) {
        case PermissionState::NotDetermined:
            return askedThisSession ? PermissionPrompt::None : PermissionPrompt::ExplainThenRequest;
        case PermissionState::Denied:
        case PermissionState::Restricted:
        case PermissionState::ServicesDisabled:
            return PermissionPrompt::PointToSettings;
        case PermissionState::NotRequired:
        case PermissionState::Granted:
            return PermissionPrompt::None;
    }
    return PermissionPrompt::None;
}

ChangeTracker::Update ChangeTracker::apply(const WifiReading &input) {
    WifiReading reading = input;
    if (reading.state == ReadState::Connected && reading.ssid.isEmpty()) {
        reading = NotConnected(reading.detail, reading.source);  // an empty name is "no network"
    }
    Update u;
    u.readingChanged = !hasLast_ || last_ != reading;
    last_ = reading;
    hasLast_ = true;
    // Only a definite answer moves the SSID; Unavailable/PermissionNeeded keep it.
    if (reading.state == ReadState::Connected || reading.state == ReadState::NotConnected) {
        const QString next = reading.state == ReadState::Connected ? reading.ssid : QString();
        if (next != ssid_) {
            ssid_ = next;
            u.ssidChanged = true;
        }
    }
    u.ssid = ssid_;
    return u;
}

QString ChangeTracker::ssid() const { return ssid_; }
WifiReading ChangeTracker::last() const { return last_; }

} // namespace ProxorWifi
