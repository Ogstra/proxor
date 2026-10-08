#include "fake_portal.hpp"
#include "sys/linux/PortalBackground.hpp"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QtTest>

using namespace ProxorDesktop;

namespace {
const char kBackground[] = "org.freedesktop.portal.Background";

struct Got {
    int count = 0;
    AutostartResult r;
};
} // namespace

class PortalBackgroundTest : public QObject {
    Q_OBJECT

    Got run(uint version, bool enable, int timeoutMs = 3000) {
        Got g;
        QObject ctx;
        QEventLoop loop;
        RequestAutostartWith(XdgPortalClient(QDBusConnection::sessionBus()), version, enable,
                             {"proxor", "-tray", "-appdata"}, QStringLiteral("Start Proxor in the tray."), &ctx,
                             [&](const AutostartResult &r) {
                                 ++g.count;
                                 g.r = r;
                                 loop.quit();
                             },
                             timeoutMs);
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        return g;
    }

    static void answer(FakePortal &fake, uint response, const QVariantMap &results, const QString &err = {}) {
        fake.setRequestHandler(kBackground, "RequestBackground", [=](const FakePortalCall &) {
            FakePortalAnswer a;
            a.response = response;
            a.results = results;
            a.errorName = err;
            return a;
        });
    }

private slots:
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void versionZeroIsNotAvailableWithoutCall() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0, {});
        const Got g = run(0, true);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.r.result.outcome, PortalOutcome::NotAvailable);
        for (const auto &c : fake.calls()) QVERIFY(c.member != "RequestBackground");
    }

    void enableGranted() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        answer(fake, 0, {{"background", true}, {"autostart", true}});
        const Got g = run(1, true);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Granted);
        QVERIFY(g.r.autostart);
        QList<FakePortalCall> calls;
        for (const auto &c : fake.calls())
            if (c.member == "RequestBackground") calls << c;
        QCOMPARE(calls.size(), 1);
        QCOMPARE(calls[0].interface, QString(kBackground));
        QCOMPARE(calls[0].args.first().toString(), QString());
        const QVariantMap o = calls[0].args.last().toMap();
        QCOMPARE(o.value("autostart").toBool(), true);
        QCOMPARE(o.value("commandline").toStringList(), (QStringList{"proxor", "-tray", "-appdata"}));
        QVERIFY(o.contains("dbus-activatable"));
        QCOMPARE(o.value("dbus-activatable").toBool(), false);
        QVERIFY(!o.value("reason").toString().isEmpty());
        QVERIFY(o.value("handle_token").toString().startsWith("proxor_"));
    }

    void disableGranted() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        answer(fake, 0, {{"autostart", false}});
        const Got g = run(1, false);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Granted);
        QVERIFY(!g.r.autostart);
        const QVariantMap o = fake.calls().last().args.last().toMap();
        QCOMPARE(o.value("autostart").toBool(), false);
    }

    void enableButDesktopSaysNo() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        answer(fake, 0, {{"background", true}, {"autostart", false}});
        const Got g = run(1, true);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Denied);
        QVERIFY(g.r.result.detail.contains("did not allow"));
    }

    void cancelledDeniedFailed() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        answer(fake, 1, {});
        QCOMPARE(run(1, true).r.result.outcome, PortalOutcome::Cancelled);
        QCOMPARE(run(1, true).r.result.detail, QString("The request was cancelled."));
        answer(fake, 2, {});
        QCOMPARE(run(1, true).r.result.outcome, PortalOutcome::Denied);
        answer(fake, 0, {}, "org.freedesktop.portal.Error.NotAllowed");
        const Got g = run(1, true);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Failed);
        QVERIFY(!g.r.result.detail.isEmpty());
    }

    void timesOut() {
        FakePortal fake;
        QVERIFY(fake.start());
        fake.setVersion(kBackground, 1);
        fake.setRequestHandler(kBackground, "RequestBackground", [](const FakePortalCall &) {
            FakePortalAnswer a;
            a.emitResponse = false;
            return a;
        });
        const Got g = run(1, true, 400);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.r.result.outcome, PortalOutcome::TimedOut);
    }
};

QTEST_MAIN(PortalBackgroundTest)
#include "portal_background_test.moc"
