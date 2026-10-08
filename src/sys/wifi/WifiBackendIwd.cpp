#include "sys/wifi/WifiBackendIwd.hpp"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <algorithm>

using namespace ProxorWifi;

namespace {

const char kService[] = "net.connman.iwd";
const char kSource[] = "iwd";

void RegisterIwdMetaTypes() {
    static const bool done = [] {
        qDBusRegisterMetaType<IwdInterfaces>();
        qDBusRegisterMetaType<IwdManagedObjects>();
        return true;
    }();
    Q_UNUSED(done);
}

// No user-visible wording for any non-answer: the chain shows the NetworkManager reading instead.
WifiReading NoAnswer() { return Unavailable(QString(), QString::fromLatin1(kSource)); }

// Maps a D-Bus error to how iwd answered. AccessDenied means iwd runs but its policy refuses
// this user, so it is neutral and NOT "iwd is not running".
IwdPresence ClassifyError(const QDBusError &err) {
    const QString n = err.name();
    if (n == QLatin1String("org.freedesktop.DBus.Error.ServiceUnknown") ||
        n == QLatin1String("org.freedesktop.DBus.Error.NameHasNoOwner") ||
        n == QLatin1String("org.freedesktop.DBus.Error.Disconnected") ||
        n == QLatin1String("org.freedesktop.DBus.Error.NoServer") ||
        err.type() == QDBusError::ServiceUnknown || err.type() == QDBusError::Disconnected || err.type() == QDBusError::NoServer)
        return IwdPresence::Unreachable;
    if (n == QLatin1String("org.freedesktop.DBus.Error.AccessDenied") || err.type() == QDBusError::AccessDenied)
        return IwdPresence::Denied;
    if (n == QLatin1String("org.freedesktop.DBus.Error.NoReply") || err.type() == QDBusError::NoReply)
        return IwdPresence::TimedOut;
    return IwdPresence::Failed;
}

QString ObjectPathOf(const QVariant &v) {
    if (v.canConvert<QDBusObjectPath>()) return v.value<QDBusObjectPath>().path();
    return v.toString();
}

} // namespace

IwdWifiReader::IwdWifiReader(QDBusConnection bus, bool sandboxed, int budgetMs)
    : bus_(std::move(bus)), sandboxed_(sandboxed), budgetMs_(budgetMs) {}

WifiReading IwdWifiReader::read() { return readWithin(budgetMs_); }

IwdPresence IwdWifiReader::presence() const { return presence_; }

WifiReading IwdWifiReader::readWithin(int budgetMs) {
    const QString src = QString::fromLatin1(kSource);
    if (sandboxed_) {
        presence_ = IwdPresence::Sandboxed;  // iwd lives on the system bus: no bus call from the Flatpak
        return NoAnswer();
    }
    if (!bus_.isConnected()) {
        presence_ = IwdPresence::Unreachable;
        return NoAnswer();
    }
    RegisterIwdMetaTypes();

    QDBusMessage msg = QDBusMessage::createMethodCall(QLatin1String(kService), QStringLiteral("/"),
                                                      QStringLiteral("org.freedesktop.DBus.ObjectManager"),
                                                      QStringLiteral("GetManagedObjects"));
    msg.setAutoStartService(false);  // a host without a running iwd must never get it D-Bus-activated by Proxor
    const QDBusMessage reply = bus_.call(msg, QDBus::Block, std::max(200, budgetMs));
    if (reply.type() != QDBusMessage::ReplyMessage) {
        presence_ = ClassifyError(QDBusError(reply));
        return NoAnswer();
    }
    if (reply.arguments().isEmpty() || !reply.arguments().first().canConvert<QDBusArgument>()) {
        presence_ = IwdPresence::Failed;
        return NoAnswer();
    }
    const QDBusArgument arg = reply.arguments().first().value<QDBusArgument>();
    if (arg.currentSignature() != QLatin1String("a{oa{sa{sv}}}")) {
        presence_ = IwdPresence::Failed;
        return NoAnswer();
    }
    const IwdManagedObjects objects = qdbus_cast<IwdManagedObjects>(arg);
    presence_ = IwdPresence::Present;

    const QString ifStation = QStringLiteral("net.connman.iwd.Station");
    const QString ifNetwork = QStringLiteral("net.connman.iwd.Network");
    const QString ifDevice = QStringLiteral("net.connman.iwd.Device");

    int stations = 0;
    int devices = 0;
    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {  // object-path order
        if (it.value().contains(ifDevice)) ++devices;
        const auto st = it.value().constFind(ifStation);
        if (st == it.value().constEnd()) continue;
        ++stations;
        const QString state = st->value(QStringLiteral("State")).toString();
        if (state != QLatin1String("connected") && state != QLatin1String("roaming")) continue;
        const QString netPath = ObjectPathOf(st->value(QStringLiteral("ConnectedNetwork")));
        if (netPath.isEmpty() || netPath == QLatin1String("/")) continue;
        const auto net = objects.constFind(QDBusObjectPath(netPath));
        if (net == objects.constEnd()) continue;
        const auto props = net->constFind(ifNetwork);
        if (props == net->constEnd()) continue;
        const QString name = props->value(QStringLiteral("Name")).toString();
        if (!name.isEmpty()) return Connected(name, src);
    }
    if (stations > 0 || devices > 0) return NotConnected(QString(), src);
    return NotConnected(QCoreApplication::translate("IwdWifiReader", "No Wi-Fi adapter found."), src);
}
