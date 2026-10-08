#include "fake_portal.hpp"
#include "sys/linux/PortalGlobalShortcuts.hpp"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QtTest>

using namespace ProxorDesktop;

namespace {
const char kShortcuts[] = "org.freedesktop.portal.GlobalShortcuts";
const char kSession[] = "org.freedesktop.portal.Session";
const char kPortalPath[] = "/org/freedesktop/portal/desktop";

struct BindOutcome {
    int count = 0;
    QList<BoundShortcut> bound;
    PortalResult result;
};

QList<ShortcutRequest> TwoShortcuts() {
    return {{"show-main-window", "Show main window", "CTRL+ALT+p"}, {"manage-groups", "Manage groups", ""}};
}
} // namespace

class PortalShortcutsTest : public QObject {
    Q_OBJECT

    // Fake with CreateSession answered {session_handle}; bindAnswer drives BindShortcuts.
    void setup(FakePortal &fake, uint createResponse, std::function<FakePortalAnswer(const FakePortalCall &)> bindAnswer) {
        fake.setVersion(kShortcuts, 1);
        fake.setRequestHandler(kShortcuts, "CreateSession", [createResponse](const FakePortalCall &c) {
            FakePortalAnswer a;
            a.response = createResponse;
            if (createResponse == 0) {
                const QString token = c.args.last().toMap().value("session_handle_token").toString();
                a.results.insert("session_handle", XdgPortalClient::SessionPath(c.sender, token));
            }
            return a;
        });
        if (bindAnswer) fake.setRequestHandler(kShortcuts, "BindShortcuts", bindAnswer);
        fake.setDirectHandler(kSession, "Close", [](const QDBusMessage &m) { return m.createReply(); });
    }

    BindOutcome bindAndWait(GlobalShortcutSession &s, const QList<ShortcutRequest> &req) {
        BindOutcome out;
        QEventLoop loop;
        s.bind(req, "x11:1", [&](const QList<BoundShortcut> &b, const PortalResult &r) {
            ++out.count;
            out.bound = b;
            out.result = r;
            loop.quit();
        });
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        loop.exec();
        return out;
    }

    static QVariantMap BoundResults(const QStringList &ids) {
        QList<PortalShortcut> list;
        for (const QString &id : ids) {
            PortalShortcut p;
            p.id = id;
            p.options.insert("trigger_description", id == "show-main-window" ? "Ctrl+Alt+P" : "Ctrl+G");
            list << p;
        }
        QVariantMap m;
        m.insert("shortcuts", QVariant::fromValue(list));
        return m;
    }

private slots:
    void initTestCase() { RegisterPortalShortcutTypes(); }
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void versionZeroIsNull() {
        FakePortal fake;
        QVERIFY(fake.start());
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 0, &ctx);
        QVERIFY(!s);
        QVERIFY(fake.calls().isEmpty());
    }

    void createSessionCarriesTokens() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.results = BoundResults({"show-main-window"}); return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        QVERIFY(s);
        QTRY_VERIFY(!fake.calls().isEmpty());
        const auto c = fake.calls().first();
        QCOMPARE(c.member, QString("CreateSession"));
        const QVariantMap opts = c.args.last().toMap();
        QVERIFY(opts.value("handle_token").toString().startsWith("proxor_"));
        QVERIFY(opts.value("session_handle_token").toString().startsWith("proxor_"));
    }

    void queuedBindIsSentWithShortcuts() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.results = BoundResults({"show-main-window"}); return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        QVERIFY(s);
        const BindOutcome o = bindAndWait(*s, TwoShortcuts()); // issued before the session Response
        QCOMPARE(o.count, 1);
        QCOMPARE(o.result.outcome, PortalOutcome::Granted);
        QCOMPARE(o.bound.size(), 1);
        QCOMPARE(o.bound.first().id, QString("show-main-window"));
        QCOMPARE(o.bound.first().triggerDescription, QString("Ctrl+Alt+P"));

        FakePortalCall bind, create;
        for (const auto &c : fake.calls()) {
            if (c.member == "BindShortcuts") bind = c;
            if (c.member == "CreateSession") create = c;
        }
        QCOMPARE(bind.member, QString("BindShortcuts"));
        const QString handle = XdgPortalClient::SessionPath(create.sender, create.args.last().toMap().value("session_handle_token").toString());
        QCOMPARE(bind.args.at(0).value<QDBusObjectPath>().path(), handle);
        const auto list = qdbus_cast<QList<PortalShortcut>>(bind.args.at(1).value<QDBusArgument>());
        QCOMPARE(list.size(), 2);
        QCOMPARE(list[0].id, QString("show-main-window"));
        QCOMPARE(list[0].options.value("description").toString(), QString("Show main window"));
        QCOMPARE(list[0].options.value("preferred_trigger").toString(), QString("CTRL+ALT+p"));
        QCOMPARE(list[1].id, QString("manage-groups"));
        QVERIFY(!list[1].options.contains("preferred_trigger"));
        QCOMPARE(bind.args.at(2).toString(), QString("x11:1"));
        QVERIFY(bind.args.last().toMap().contains("handle_token"));
    }

    void bindCancelled() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.response = 1; return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        const BindOutcome o = bindAndWait(*s, TwoShortcuts());
        QCOMPARE(o.result.outcome, PortalOutcome::Cancelled);
        QCOMPARE(o.result.detail, QString("The desktop dialog was closed, so no global hotkey is active."));
        QVERIFY(o.bound.isEmpty());
    }

    void bindDenied() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.response = 2; return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        const BindOutcome o = bindAndWait(*s, TwoShortcuts());
        QCOMPARE(o.result.outcome, PortalOutcome::Denied);
    }

    void bindFailsOnDBusError() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.errorName = "org.freedesktop.DBus.Error.Failed"; return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        const BindOutcome o = bindAndWait(*s, TwoShortcuts());
        QCOMPARE(o.result.outcome, PortalOutcome::Failed);
        QVERIFY(!o.result.detail.isEmpty());
    }

    void bindTimesOut() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.emitResponse = false; return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx, 400);
        const BindOutcome o = bindAndWait(*s, TwoShortcuts());
        QCOMPARE(o.result.outcome, PortalOutcome::TimedOut);
    }

    void createSessionDeniedReportsOnBind() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 2, [](const FakePortalCall &) { FakePortalAnswer a; return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        const BindOutcome o = bindAndWait(*s, TwoShortcuts());
        QCOMPARE(o.result.outcome, PortalOutcome::Denied);
        const BindOutcome later = bindAndWait(*s, TwoShortcuts()); // a later bind reports it too
        QCOMPARE(later.result.outcome, PortalOutcome::Denied);
        for (const auto &c : fake.calls()) QVERIFY(c.member != "BindShortcuts");
    }

    void activatedForOurSessionOnly() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.results = BoundResults({"show-main-window"}); return a; });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        QStringList got;
        s->onActivated = [&](const QString &id) { got << id; };
        QVERIFY(bindAndWait(*s, TwoShortcuts()).result.outcome == PortalOutcome::Granted);
        FakePortalCall create;
        for (const auto &c : fake.calls()) if (c.member == "CreateSession") create = c;
        const QString handle = XdgPortalClient::SessionPath(create.sender, create.args.last().toMap().value("session_handle_token").toString());
        auto emitFor = [&](const QString &path, const QString &id) {
            fake.emitSignal(kPortalPath, kShortcuts, "Activated",
                            {QVariant::fromValue(QDBusObjectPath(path)), id, QVariant::fromValue(qulonglong(0)), QVariant::fromValue(QVariantMap())});
        };
        emitFor("/org/freedesktop/portal/desktop/session/1_999/other", "ignored");
        emitFor(handle, "manage-groups");
        QTRY_COMPARE(got.size(), 1);
        QTest::qWait(150);
        QCOMPARE(got, QStringList{"manage-groups"});
    }

    void destructorClosesAsynchronously() {
        FakePortal fake;
        QVERIFY(fake.start());
        setup(fake, 0, [](const FakePortalCall &) { FakePortalAnswer a; a.results = BoundResults({"show-main-window"}); return a; });
        fake.setDirectHandler(kSession, "Close", [](const QDBusMessage &m) {
            QThread::msleep(1000);
            return m.createReply();
        });
        QObject ctx;
        auto s = CreateGlobalShortcutSessionWith(XdgPortalClient(QDBusConnection::sessionBus()), 1, &ctx);
        int activations = 0;
        s->onActivated = [&](const QString &) { ++activations; };
        QVERIFY(bindAndWait(*s, TwoShortcuts()).result.outcome == PortalOutcome::Granted);
        FakePortalCall create;
        for (const auto &c : fake.calls()) if (c.member == "CreateSession") create = c;
        const QString handle = XdgPortalClient::SessionPath(create.sender, create.args.last().toMap().value("session_handle_token").toString());

        QElapsedTimer t;
        t.start();
        s.reset();
        QVERIFY2(t.elapsed() < 100, qPrintable(QString::number(t.elapsed())));
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            for (const auto &c : fake.calls())
                if (c.member == "Close" && c.interface == kSession && c.path == handle) return true;
            return false;
        }(), 3000);
        fake.emitSignal(kPortalPath, kShortcuts, "Activated",
                        {QVariant::fromValue(QDBusObjectPath(handle)), QString("manage-groups"), QVariant::fromValue(qulonglong(0)), QVariant::fromValue(QVariantMap())});
        QTest::qWait(200);
        QCOMPARE(activations, 0);
    }
};

QTEST_MAIN(PortalShortcutsTest)
#include "portal_shortcuts_test.moc"
