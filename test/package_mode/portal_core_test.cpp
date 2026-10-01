#include "fake_portal.hpp"
#include "sys/linux/XdgPortal.hpp"

#include <QDBusConnection>
#include <QDBusObjectPath>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QtTest>

using namespace ProxorDesktop;

namespace {
const char kScreenshot[] = "org.freedesktop.portal.Screenshot";
const char kBackground[] = "org.freedesktop.portal.Background";
const char kShortcuts[] = "org.freedesktop.portal.GlobalShortcuts";

struct Got {
    int count = 0;
    PortalReply reply;
    qint64 elapsedMs = 0;
};
} // namespace

class PortalCoreTest : public QObject {
    Q_OBJECT

    // Runs request() and waits until done ran (or 3 s).
    Got run(const XdgPortalClient &c, int timeoutMs, QObject *ctx, bool *syncFlag = nullptr) {
        Got g;
        QElapsedTimer t;
        t.start();
        QEventLoop loop;
        bool returned = false;
        c.request(QString::fromLatin1(kScreenshot), QStringLiteral("Screenshot"), {QString(), }, {}, timeoutMs, ctx,
                  [&](const PortalReply &r) {
                      if (syncFlag) *syncFlag = returned;
                      ++g.count;
                      g.reply = r;
                      g.elapsedMs = t.elapsed();
                      loop.quit();
                  });
        returned = true;
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        loop.exec();
        return g;
    }

private slots:
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void paths() {
        QCOMPARE(XdgPortalClient::RequestPath(":1.42", "proxor_x"), QString("/org/freedesktop/portal/desktop/request/1_42/proxor_x"));
        QCOMPARE(XdgPortalClient::SessionPath(":1.42", "proxor_x"), QString("/org/freedesktop/portal/desktop/session/1_42/proxor_x"));
    }

    void tokens() {
        const QString a = XdgPortalClient::NewToken(), b = XdgPortalClient::NewToken();
        QVERIFY(QRegularExpression("^proxor_[a-z0-9]{16}$").match(a).hasMatch());
        QVERIFY(a != b);
    }

    void versionKnownAndUnknown() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kScreenshot, 2);
        XdgPortalClient c(QDBusConnection::sessionBus());
        QCOMPARE(c.interfaceVersion(kScreenshot, 1000), 2u);
        QCOMPARE(c.interfaceVersion("org.freedesktop.portal.Nope", 1000), 0u);
    }

    void versionWithoutFakeIsZeroFast() {
        XdgPortalClient c(QDBusConnection::sessionBus());
        QElapsedTimer t;
        t.start();
        QCOMPARE(c.interfaceVersion(kScreenshot, 1000), 0u);
        QVERIFY2(t.elapsed() < 700, qPrintable(QString::number(t.elapsed())));
    }

    void probeRegistersBeforeReading() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        fake.setVersion(kScreenshot, 2);
        fake.setVersion(kShortcuts, 1);
        fake.setDirectHandler("org.freedesktop.host.portal.Registry", "Register", [](const QDBusMessage &m) { return m.createReply(); });
        XdgPortalClient c(QDBusConnection::sessionBus());
        const PortalVersions v = ProbePortalVersions(c, false, "proxor", 1500);
        QCOMPARE(v.background, 1u);
        QCOMPARE(v.screenshot, 2u);
        QCOMPARE(v.globalShortcuts, 1u);
        const auto calls = fake.calls();
        int reg = -1, get = -1;
        for (int i = 0; i < calls.size(); ++i) {
            if (reg < 0 && calls[i].member == "Register") reg = i;
            if (get < 0 && calls[i].member == "Get") get = i;
        }
        QVERIFY(reg >= 0);
        QVERIFY(get > reg);
        QCOMPARE(calls[reg].args.value(0).toString(), QString("proxor"));
    }

    void probeSandboxedDoesNotRegister() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kScreenshot, 2);
        XdgPortalClient c(QDBusConnection::sessionBus());
        const PortalVersions v = ProbePortalVersions(c, true, "proxor", 1500);
        QCOMPARE(v.screenshot, 2u);
        for (const auto &call : fake.calls()) QVERIFY(call.member != "Register");
    }

    void probeSurvivesRegisterUnknownMethod() {
        FakePortal fake; // no Register handler: the fake answers UnknownMethod
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        XdgPortalClient c(QDBusConnection::sessionBus());
        QCOMPARE(ProbePortalVersions(c, false, "proxor", 1500).background, 1u);
    }

    void requestGranted() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.results.insert("uri", "file:///tmp/a.png");
            return a;
        });
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        const Got g = run(c, 2000, &ctx);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.reply.response, 0u);
        QCOMPARE(g.reply.results.value("uri").toString(), QString("file:///tmp/a.png"));
        QCOMPARE(ResultFromReply(g.reply).outcome, PortalOutcome::Granted);
        const auto calls = fake.calls();
        QVERIFY(!calls.isEmpty());
        QVERIFY(calls.last().args.last().toMap().value("handle_token").toString().startsWith("proxor_"));
    }

    void requestCancelled() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.response = 1;
            return a;
        });
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        const Got g = run(c, 2000, &ctx);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.reply.response, 1u);
        QCOMPARE(ResultFromReply(g.reply).outcome, PortalOutcome::Cancelled);
    }

    void requestDeniedMapsToDenied() {
        PortalReply r;
        r.response = 2;
        const PortalResult res = ResultFromReply(r);
        QCOMPARE(res.outcome, PortalOutcome::Denied);
        QCOMPARE(res.detail, QString("The desktop refused or could not complete the request."));
    }

    void differentRequestPathStillDelivered() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.overrideRequestPath = "/org/freedesktop/portal/desktop/request/9_9/other";
            a.delayMs = 150;
            a.results.insert("k", "v");
            return a;
        });
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        const Got g = run(c, 2000, &ctx);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.reply.response, 0u);
        QCOMPARE(g.reply.results.value("k").toString(), QString("v"));
    }

    void requestDbusError() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.errorName = "org.freedesktop.portal.Error.NotAllowed";
            return a;
        });
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        const Got g = run(c, 2000, &ctx);
        QCOMPARE(g.count, 1);
        QVERIFY(!g.reply.error.isEmpty());
        const PortalResult res = ResultFromReply(g.reply);
        QCOMPARE(res.outcome, PortalOutcome::Failed);
        QCOMPARE(res.detail, g.reply.error);
    }

    void requestTimesOut() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.emitResponse = false;
            return a;
        });
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        const Got g = run(c, 400, &ctx);
        QCOMPARE(g.count, 1);
        QVERIFY(g.reply.timedOut);
        QVERIFY2(g.elapsedMs >= 380 && g.elapsedMs <= 1400, qPrintable(QString::number(g.elapsedMs)));
        QCOMPARE(ResultFromReply(g.reply).outcome, PortalOutcome::TimedOut);
    }

    void contextDeletedBeforeResponse() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.delayMs = 300;
            return a;
        });
        XdgPortalClient c(QDBusConnection::sessionBus());
        auto *ctx = new QObject;
        int called = 0;
        c.request(kScreenshot, "Screenshot", {QString()}, {}, 2000, ctx, [&](const PortalReply &) { ++called; });
        delete ctx;
        QTest::qWait(700);
        QCOMPARE(called, 0);
    }

    void doneIsNeverSynchronous() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setRequestHandler(kScreenshot, "Screenshot", [](const FakePortalCall &) { return FakePortalAnswer(); });
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        bool flagWhenRan = false;
        const Got g = run(c, 2000, &ctx, &flagWhenRan);
        QCOMPARE(g.count, 1);
        QVERIFY(flagWhenRan);
    }
};

QTEST_MAIN(PortalCoreTest)
#include "portal_core_test.moc"
