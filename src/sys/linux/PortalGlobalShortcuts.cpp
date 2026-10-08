// org.freedesktop.portal.GlobalShortcuts: one session, bind() requests, Activated signals. QtCore + QtDBus only.
#include "PortalGlobalShortcuts.hpp"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QPointer>
#include <QTimer>

namespace ProxorDesktop {

namespace {

const char kShortcutsIface[] = "org.freedesktop.portal.GlobalShortcuts";
const char kSessionIface[] = "org.freedesktop.portal.Session";
const char kPortalPath[] = "/org/freedesktop/portal/desktop";
const char kPortalService[] = "org.freedesktop.portal.Desktop";

PortalResult BindResultFromReply(const PortalReply &reply) {
    PortalResult r = ResultFromReply(reply);
    if (r.outcome == PortalOutcome::Cancelled) r.detail = QStringLiteral("The desktop dialog was closed, so no global hotkey is active.");
    return r;
}

QList<PortalShortcut> ShortcutsFromVariant(const QVariant &v) {
    if (v.canConvert<QDBusArgument>()) return qdbus_cast<QList<PortalShortcut>>(v.value<QDBusArgument>());
    if (v.canConvert<QList<PortalShortcut>>()) return v.value<QList<PortalShortcut>>();
    return {};
}

using BindDone = std::function<void(const QList<BoundShortcut> &, const PortalResult &)>;

// Owns the D-Bus state; lives on the context's thread (child of the context when there is one).
class ShortcutReceiver : public QObject {
    Q_OBJECT
public:
    struct Pending {
        QList<ShortcutRequest> shortcuts;
        QString parentWindow;
        BindDone done;
    };

    XdgPortalClient client;
    int timeoutMs;
    GlobalShortcutSession *owner = nullptr;
    QString handle;
    bool ready = false;
    bool failed = false;
    PortalResult failure;
    QList<Pending> pending;

    ShortcutReceiver(const XdgPortalClient &c, int timeout, QObject *parent) : QObject(parent), client(c), timeoutMs(timeout) {
        client.bus().connect(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath), QString::fromLatin1(kShortcutsIface),
                             QStringLiteral("Activated"), this, SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
        QVariantMap options;
        options.insert(QStringLiteral("session_handle_token"), XdgPortalClient::NewToken());
        client.request(QString::fromLatin1(kShortcutsIface), QStringLiteral("CreateSession"), {}, options, timeoutMs, this,
                       [this](const PortalReply &reply) { onSessionCreated(reply); });
    }

    ~ShortcutReceiver() override {
        client.bus().disconnect(QString::fromLatin1(kPortalService), QString::fromLatin1(kPortalPath), QString::fromLatin1(kShortcutsIface),
                                QStringLiteral("Activated"), this, SLOT(onActivated(QDBusObjectPath, QString, qulonglong, QVariantMap)));
        if (!handle.isEmpty()) {
            // Fire and forget: never block the UI thread (shutdown, re-registration) on the portal.
            QDBusMessage m = QDBusMessage::createMethodCall(QString::fromLatin1(kPortalService), handle, QString::fromLatin1(kSessionIface),
                                                            QStringLiteral("Close"));
            client.bus().asyncCall(m, 500);
        }
    }

    void bind(const QList<ShortcutRequest> &shortcuts, const QString &parentWindow, BindDone done) {
        if (failed) {
            reportLater(std::move(done), failure);
            return;
        }
        if (!ready) {
            pending.append({shortcuts, parentWindow, std::move(done)});
            return;
        }
        send(shortcuts, parentWindow, std::move(done));
    }

private:
    void reportLater(BindDone done, const PortalResult &r) {
        QTimer::singleShot(0, this, [done, r] { done({}, r); });
    }

    void onSessionCreated(const PortalReply &reply) {
        PortalResult r = BindResultFromReply(reply);
        QString h;
        if (r.outcome == PortalOutcome::Granted) {
            const QVariant v = reply.results.value(QStringLiteral("session_handle"));
            h = v.canConvert<QDBusObjectPath>() ? v.value<QDBusObjectPath>().path() : v.toString();
            if (h.isEmpty()) {
                r.outcome = PortalOutcome::Failed;
                r.detail = QStringLiteral("The desktop did not return a global shortcuts session.");
            }
        }
        const QList<Pending> queued = std::exchange(pending, {});
        if (r.outcome != PortalOutcome::Granted) {
            failed = true;
            failure = r;
            for (const Pending &p : queued) p.done({}, failure);
            return;
        }
        handle = h;
        ready = true;
        for (const Pending &p : queued) send(p.shortcuts, p.parentWindow, p.done);
    }

    void send(const QList<ShortcutRequest> &shortcuts, const QString &parentWindow, BindDone done) {
        QList<PortalShortcut> list;
        for (const ShortcutRequest &s : shortcuts) {
            PortalShortcut p;
            p.id = s.id;
            p.options.insert(QStringLiteral("description"), s.description);
            if (!s.preferredTrigger.isEmpty()) p.options.insert(QStringLiteral("preferred_trigger"), s.preferredTrigger);
            list << p;
        }
        client.request(QString::fromLatin1(kShortcutsIface), QStringLiteral("BindShortcuts"),
                       {QVariant::fromValue(QDBusObjectPath(handle)), QVariant::fromValue(list), parentWindow}, {}, timeoutMs, this,
                       [done](const PortalReply &reply) {
                           const PortalResult r = BindResultFromReply(reply);
                           QList<BoundShortcut> bound;
                           if (r.outcome == PortalOutcome::Granted) {
                               for (const PortalShortcut &p : ShortcutsFromVariant(reply.results.value(QStringLiteral("shortcuts"))))
                                   bound.append({p.id, p.options.value(QStringLiteral("trigger_description")).toString()});
                           }
                           done(bound, r);
                       });
    }

public slots:
    void onActivated(const QDBusObjectPath &session, const QString &shortcutId, qulonglong, const QVariantMap &) {
        if (handle.isEmpty() || session.path() != handle) return;
        if (owner && owner->onActivated) owner->onActivated(shortcutId);
    }
};

class ShortcutSession : public GlobalShortcutSession {
public:
    QPointer<ShortcutReceiver> receiver;
    ShortcutSession(const XdgPortalClient &c, int timeoutMs, QObject *context) {
        receiver = new ShortcutReceiver(c, timeoutMs, context);
        receiver->owner = this;
    }
    ~ShortcutSession() override {
        if (receiver) {
            receiver->owner = nullptr;
            delete receiver.data();
        }
    }
    void bind(const QList<ShortcutRequest> &shortcuts, const QString &parentWindow, BindDone done) override {
        if (!receiver) {
            done({}, PortalResult{PortalOutcome::Failed, QStringLiteral("The global shortcuts session is gone.")});
            return;
        }
        receiver->bind(shortcuts, parentWindow, std::move(done));
    }
};

} // namespace

QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s) {
    arg.beginStructure();
    arg << s.id << s.options;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s) {
    arg.beginStructure();
    arg >> s.id >> s.options;
    arg.endStructure();
    return arg;
}

void RegisterPortalShortcutTypes() {
    static const bool done = [] {
        qDBusRegisterMetaType<PortalShortcut>();
        qDBusRegisterMetaType<QList<PortalShortcut>>();
        return true;
    }();
    (void)done;
}

std::unique_ptr<GlobalShortcutSession> CreateGlobalShortcutSessionWith(const XdgPortalClient &client, uint version, QObject *context,
                                                                       int timeoutMs) {
    if (version == 0) return nullptr;
    RegisterPortalShortcutTypes();
    return std::make_unique<ShortcutSession>(client, timeoutMs, context);
}

std::unique_ptr<GlobalShortcutSession> CreateGlobalShortcutSession(QObject *context) {
    return CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), Portals().globalShortcuts, context);
}

} // namespace ProxorDesktop

#include "PortalGlobalShortcuts.moc"
