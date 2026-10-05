#include "sys/wifi/WifiBackendNetworkManager.hpp"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <algorithm>

using namespace ProxorWifi;

namespace {

const char kService[] = "org.freedesktop.NetworkManager";
const char kRootPath[] = "/org/freedesktop/NetworkManager";
const char kSource[] = "NetworkManager";

constexpr quint32 kNmDeviceStateUnmanaged = 10;

QString Tr(const char *text) { return QCoreApplication::translate("NetworkManagerWifiReader", text); }

QString NoNetworkManagerHint() {
    return Tr("Proxor reads the Wi-Fi network from NetworkManager; Wi-Fi run by iwd, wpa_supplicant or ConnMan without NetworkManager is not supported.");
}

struct CallResult {
    bool ok = false;
    bool unreachable = false;  // service missing / denied / bus down
    bool timedOut = false;
    QVariant value;
};

bool IsUnreachable(const QDBusError &err) {
    const QString n = err.name();
    return n == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown") ||
           n == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner") ||
           n == QLatin1String("org.freedesktop.DBus.Error.AccessDenied") ||
           n == QLatin1String("org.freedesktop.DBus.Error.Disconnected") ||
           n == QLatin1String("org.freedesktop.DBus.Error.NoServer");
}

class Caller {
public:
    Caller(QDBusConnection &bus, int budgetMs) : bus_(bus), budgetMs_(budgetMs) { timer_.start(); }

    bool expired() const { return timer_.elapsed() >= budgetMs_; }

    CallResult call(const QString &path, const QString &iface, const QString &method, const QVariantList &args) {
        CallResult r;
        if (expired()) {
            r.timedOut = true;
            return r;
        }
        const int remaining = int(std::max<qint64>(200, budgetMs_ - timer_.elapsed()));
        QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(kService), path, iface, method);
        msg.setArguments(args);
        const QDBusMessage reply = bus_.call(msg, QDBus::Block, remaining);
        if (reply.type() == QDBusMessage::ReplyMessage) {
            r.ok = true;
            if (!reply.arguments().isEmpty()) r.value = reply.arguments().first();
            return r;
        }
        const QDBusError err(reply);
        if (IsUnreachable(err)) r.unreachable = true;
        else if (err.type() == QDBusError::NoReply || err.name() == QLatin1String("org.freedesktop.DBus.Error.NoReply"))
            r.timedOut = true;
        return r;
    }

    CallResult get(const QString &path, const QString &iface, const QString &prop) {
        CallResult r = call(path, QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Get"), {iface, prop});
        if (!r.ok) return r;
        QVariant v = r.value;
        if (v.canConvert<QDBusVariant>()) v = v.value<QDBusVariant>().variant();
        r.value = v;
        return r;
    }

private:
    QDBusConnection &bus_;
    int budgetMs_;
    QElapsedTimer timer_;
};

QString ObjectPathOf(const QVariant &v) {
    if (v.canConvert<QDBusObjectPath>()) return v.value<QDBusObjectPath>().path();
    return v.toString();
}

QList<QString> DevicePaths(const QVariant &v) {
    QList<QString> out;
    if (v.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        arg.beginArray();
        while (!arg.atEnd()) {
            QDBusObjectPath p;
            arg >> p;
            out << p.path();
        }
        arg.endArray();
    } else if (v.canConvert<QList<QDBusObjectPath>>()) {
        for (const auto &p : v.value<QList<QDBusObjectPath>>()) out << p.path();
    }
    return out;
}

QByteArray BytesOf(const QVariant &v) {
    if (v.typeId() == QMetaType::QByteArray) return v.toByteArray();
    if (v.canConvert<QDBusArgument>()) {
        const QDBusArgument arg = v.value<QDBusArgument>();
        QByteArray b;
        arg >> b;
        return b;
    }
    return v.toByteArray();
}

} // namespace

NetworkManagerWifiReader::NetworkManagerWifiReader(QDBusConnection bus, QString nmcliProgram, bool sandboxed, int budgetMs)
    : bus_(std::move(bus)), nmcliProgram_(std::move(nmcliProgram)), sandboxed_(sandboxed), budgetMs_(budgetMs) {}

WifiReading NetworkManagerWifiReader::read() {
    const QString src = QString::fromLatin1(kSource);
    auto fallback = [&]() -> WifiReading {
        if (sandboxed_)
            return PermissionNeeded(Tr("The Flatpak cannot reach NetworkManager, so Proxor cannot see the Wi-Fi network. Allow it with: flatpak override --user --system-talk-name=org.freedesktop.NetworkManager io.github.Ogstra.Proxor"), src);

        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
        proc.setProcessEnvironment(env);
        proc.start(nmcliProgram_, {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("ACTIVE,SSID"),
                                   QStringLiteral("device"), QStringLiteral("wifi"), QStringLiteral("list"),
                                   QStringLiteral("--rescan"), QStringLiteral("no")});
        const QString nmcliSrc = QStringLiteral("nmcli");
        if (!proc.waitForStarted(1500))
            return Unavailable(Tr("Proxor reads the Wi-Fi network from NetworkManager, which is not reachable here, and nmcli is not installed. Wi-Fi managed by iwd, wpa_supplicant or ConnMan alone is not supported."), nmcliSrc);
        if (!proc.waitForFinished(2000)) {
            proc.kill();
            proc.waitForFinished(500);
            return Unavailable(Tr("nmcli did not answer in time."), nmcliSrc);
        }
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            QString line;
            for (const QByteArray &l : proc.readAllStandardError().split('\n')) {
                const QString s = QString::fromUtf8(l).trimmed();
                if (!s.isEmpty()) {
                    line = s;
                    break;
                }
            }
            QString detail = Tr("nmcli failed: %1").arg(line);
            if (line.contains(QLatin1String("NetworkManager is not running"))) detail += QLatin1Char(' ') + NoNetworkManagerHint();
            return Unavailable(detail, nmcliSrc);
        }
        return ParseNmcliWifiList(proc.readAllStandardOutput());
    };

    if (!bus_.isConnected()) return fallback();

    Caller c(bus_, budgetMs_);
    const QString timeoutText = Tr("NetworkManager did not answer in time.");

    CallResult devs = c.call(QString::fromLatin1(kRootPath), QString::fromLatin1(kService), QStringLiteral("GetDevices"), {});
    if (devs.unreachable) return fallback();
    if (devs.timedOut) return Unavailable(timeoutText, src);
    if (!devs.ok) return Unavailable(Tr("NetworkManager did not answer correctly."), src);

    const QString ifDevice = QStringLiteral("org.freedesktop.NetworkManager.Device");
    const QString ifWireless = QStringLiteral("org.freedesktop.NetworkManager.Device.Wireless");
    const QString ifAp = QStringLiteral("org.freedesktop.NetworkManager.AccessPoint");

    int wifiDevices = 0;
    int unmanagedWifi = 0;
    for (const QString &dev : DevicePaths(devs.value)) {
        CallResult type = c.get(dev, ifDevice, QStringLiteral("DeviceType"));
        if (type.timedOut || c.expired()) return Unavailable(timeoutText, src);
        if (!type.ok || type.value.toUInt() != 2) continue;
        ++wifiDevices;

        CallResult state = c.get(dev, ifDevice, QStringLiteral("State"));
        if (state.timedOut) return Unavailable(timeoutText, src);
        if (state.ok && state.value.toUInt() == kNmDeviceStateUnmanaged) {
            ++unmanagedWifi;
            continue;
        }
        if (!state.ok || state.value.toUInt() != 100) continue;

        CallResult ap = c.get(dev, ifWireless, QStringLiteral("ActiveAccessPoint"));
        if (ap.timedOut) return Unavailable(timeoutText, src);
        const QString apPath = ap.ok ? ObjectPathOf(ap.value) : QString();
        if (apPath.isEmpty() || apPath == QLatin1String("/")) continue;

        CallResult ssid = c.get(apPath, ifAp, QStringLiteral("Ssid"));
        if (ssid.timedOut) return Unavailable(timeoutText, src);
        if (!ssid.ok) continue;
        const QString decoded = DecodeSsidBytes(BytesOf(ssid.value));
        if (decoded.isEmpty())
            return NotConnected(Tr("Connected to a hidden Wi-Fi network whose name NetworkManager does not report."), src);
        return Connected(decoded, src);
    }
    if (wifiDevices == 0) return NotConnected(Tr("No Wi-Fi adapter found."), src);
    if (unmanagedWifi == wifiDevices)
        return Unavailable(Tr("NetworkManager does not manage this computer's Wi-Fi adapter (another program such as iwd or wpa_supplicant runs it), so Proxor cannot see the Wi-Fi network.") + QLatin1Char(' ') + NoNetworkManagerHint(), src);
    return NotConnected(QString(), src);
}
