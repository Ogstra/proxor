#include "LogindSleep.hpp"
#include "sys/SleepWake.hpp"

#include <QDBusMessage>

LogindSleepListener::LogindSleepListener(const QDBusConnection &bus, QObject *parent) : QObject(parent), bus_(bus) {}

bool LogindSleepListener::start() {
    available_ = false;
    if (!bus_.isConnected()) {
        detail_ = QStringLiteral("no system D-Bus: sleep/wake is detected by the subscription timer only");
        return false;
    }
    // Bounded (1 s) ownership probe; never blocks the UI longer than that.
    QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
                                                      QStringLiteral("org.freedesktop.DBus"), QStringLiteral("NameHasOwner"));
    msg << QString::fromLatin1(kLogindService);
    const QDBusMessage reply = bus_.call(msg, QDBus::Block, 1000);
    if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty()) available_ = reply.arguments().first().toBool();

    // Matching on the well-known name makes the bus deliver the signal only from the current owner of
    // org.freedesktop.login1 (impostors are ignored) and follows a later owner.
    const bool ok = bus_.connect(QString::fromLatin1(kLogindService), QString::fromLatin1(kLogindPath), QString::fromLatin1(kLogindManager),
                                 QStringLiteral("PrepareForSleep"), this, SLOT(onPrepareForSleep(bool)));
    if (!ok) {
        detail_ = QStringLiteral("could not subscribe to systemd-logind PrepareForSleep: sleep/wake is detected by the subscription timer only");
        return false;
    }
    detail_ = available_ ? QStringLiteral("systemd-logind PrepareForSleep")
                         : QStringLiteral("systemd-logind not on the system bus (or not reachable from a sandbox): "
                                          "sleep/wake is detected by the subscription timer only");
    return true;
}

bool LogindSleepListener::serviceAvailable() const { return available_; }
QString LogindSleepListener::detail() const { return detail_; }

void LogindSleepListener::onPrepareForSleep(bool start) { emit sleeping(start); }

namespace ProxorSleepWake {
// No delay inhibitor lock is taken: the pre-sleep snapshot is in-memory state captured in the slot. Without
// an inhibitor PrepareForSleep(true) may be handled only after resume; the coordinator then falls back to
// the state observed at wake.
InstallResult Install(QObject *owner, std::function<void(bool sleeping)> onEvent) {
    if (!owner || !onEvent) return {false, QStringLiteral("no owner or callback")};
    auto *l = new LogindSleepListener(QDBusConnection::systemBus(), owner); // dies with the owner
    QObject::connect(l, &LogindSleepListener::sleeping, owner, [onEvent](bool s) { onEvent(s); });
    const bool ok = l->start();
    return {ok, l->detail()};
}
} // namespace ProxorSleepWake
