#include "fake_portal.hpp"
#include "sys/DesktopPortal.hpp"
#include "sys/linux/PortalScreenshot.hpp"
#include "sys/linux/XdgPortal.hpp"

#include <QDBusConnection>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QtTest>

using namespace ProxorDesktop;

namespace {
const char kScreenshot[] = "org.freedesktop.portal.Screenshot";

struct Got {
    int count = 0;
    ScreenshotResult r;
};
} // namespace

class PortalScreenshotTest : public QObject {
    Q_OBJECT

    Got run(uint version, int timeoutMs = 3000, const QString &parent = QStringLiteral("x11:abc")) {
        Got g;
        XdgPortalClient c(QDBusConnection::sessionBus());
        QObject ctx;
        QEventLoop loop;
        TakeScreenshotWith(c, version, parent, &ctx, [&](const ScreenshotResult &r) {
            ++g.count;
            g.r = r;
            loop.quit();
        }, timeoutMs);
        QTimer::singleShot(4000, &loop, &QEventLoop::quit);
        loop.exec();
        return g;
    }

    static QString makeFile(QTemporaryDir &dir) {
        const QString p = dir.path() + "/shot.png";
        QFile f(p);
        if (!f.open(QIODevice::WriteOnly)) return {};
        f.write("png");
        return p;
    }

    void answer(FakePortal &fake, uint response, const QVariantMap &results = {}, const QString &err = {}, bool emitResponse = true) {
        fake.setRequestHandler(kScreenshot, "Screenshot", [=](const FakePortalCall &) {
            FakePortalAnswer a;
            a.response = response;
            a.results = results;
            a.errorName = err;
            a.emitResponse = emitResponse;
            return a;
        });
    }

private slots:
    void init() {
        if (!QDBusConnection::sessionBus().isConnected()) QSKIP("no D-Bus session bus on this runner");
    }

    void versionZeroIsNotAvailable() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0);
        const Got g = run(0);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.r.result.outcome, PortalOutcome::NotAvailable);
        QVERIFY(!g.r.result.detail.isEmpty());
        QVERIFY(fake.calls().isEmpty());
    }

    void grantedWithTemporaryFile() {
        FakePortal fake;
        QVERIFY(fake.start());
        QTemporaryDir dir; // under QDir::tempPath()
        QVERIFY(dir.isValid());
        const QString path = makeFile(dir);
        QVERIFY(!path.isEmpty());
        answer(fake, 0, {{"uri", QUrl::fromLocalFile(path).toString()}});
        const Got g = run(2);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Granted);
        QCOMPARE(g.r.imagePath, path);
        QVERIFY(g.r.temporary);
    }

    void callShape() {
        FakePortal fake;
        QVERIFY(fake.start());
        QTemporaryDir dir;
        const QString path = makeFile(dir);
        answer(fake, 0, {{"uri", QUrl::fromLocalFile(path).toString()}});
        const Got g = run(2, 3000, "x11:1234");
        QCOMPARE(g.count, 1);
        const auto calls = fake.calls();
        QVERIFY(!calls.isEmpty());
        const FakePortalCall &c = calls.last();
        QCOMPARE(c.interface, QString(kScreenshot));
        QCOMPARE(c.member, QString("Screenshot"));
        QCOMPARE(c.args.value(0).toString(), QString("x11:1234"));
        const QVariantMap o = c.args.last().toMap();
        QVERIFY(o.contains("interactive"));
        QCOMPARE(o.value("interactive").toBool(), false);
        QVERIFY(o.contains("modal"));
        QCOMPARE(o.value("modal").toBool(), true);
        QVERIFY(o.value("handle_token").toString().startsWith("proxor_"));
    }

    void missingFileFails() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0, {{"uri", "file:///nonexistent/x.png"}});
        const Got g = run(2);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Failed);
        QCOMPARE(g.r.result.detail, QString("The desktop returned no screenshot."));
        QVERIFY(g.r.imagePath.isEmpty());
        QVERIFY(!g.r.temporary);
    }

    void noUriFails() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0);
        const Got g = run(2);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Failed);
        QCOMPARE(g.r.result.detail, QString("The desktop returned no screenshot."));
    }

    void nonFileUriFails() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0, {{"uri", "https://x/y.png"}});
        const Got g = run(2);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Failed);
        QCOMPARE(g.r.result.detail, QString("The desktop returned no screenshot."));
    }

    void cancelled() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 1);
        const Got g = run(2);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Cancelled);
        QCOMPARE(g.r.result.detail, QString("The screenshot was cancelled."));
    }

    void denied() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 2);
        const Got g = run(2);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Denied);
        QVERIFY(!g.r.result.detail.isEmpty());
    }

    void dbusError() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0, {}, "org.freedesktop.portal.Error.NotAllowed");
        const Got g = run(2);
        QCOMPARE(g.r.result.outcome, PortalOutcome::Failed);
        QVERIFY(!g.r.result.detail.isEmpty());
    }

    void timesOut() {
        FakePortal fake;
        QVERIFY(fake.start());
        answer(fake, 0, {}, {}, false);
        const Got g = run(2, 400);
        QCOMPARE(g.count, 1);
        QCOMPARE(g.r.result.outcome, PortalOutcome::TimedOut);
    }

    void temporaryPathRule() {
        QVERIFY(IsTemporaryScreenshotPath("/tmp/a.png", {"/tmp"}));
        QVERIFY(IsTemporaryScreenshotPath("/run/user/1000/a.png", {"/run/user/1000"}));
        QVERIFY(!IsTemporaryScreenshotPath("/run/user/1000/doc/abc/a.png", {"/run/user/1000"}));
        QVERIFY(!IsTemporaryScreenshotPath("/home/u/Pictures/Screenshots/a.png", {"/tmp", "/run/user/1000"}));
        QVERIFY(!IsTemporaryScreenshotPath("/tmpfoo/a.png", {"/tmp"}));
        QVERIFY(!IsTemporaryScreenshotPath("/tmp/a.png", {}));
    }
};

QTEST_MAIN(PortalScreenshotTest)
#include "portal_screenshot_test.moc"
