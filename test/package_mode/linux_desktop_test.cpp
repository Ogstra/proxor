#include "platform/LinuxDesktop.hpp"

#include <QtTest>

using namespace ProxorPlatform;

class LinuxDesktopTest : public QObject {
    Q_OBJECT

    static LinuxDesktopInfo Detect(const QString &cur, const QString &sess = {}, const QString &ds = {}, const QString &kde = {}) {
        return DetectLinuxDesktop({cur, sess, ds, kde});
    }

private slots:
    void kdePlasma6() {
        const auto d = Detect("KDE", "KDE", {}, "6");
        QCOMPARE(d.family, LinuxDesktopFamily::Kde);
        QCOMPARE(d.kdeMajor, 6);
        QCOMPARE(d.label, QString("KDE Plasma 6"));
    }
    void kdeSessionPlasmaOnly() {
        const auto d = Detect({}, "plasma");
        QCOMPARE(d.family, LinuxDesktopFamily::Kde);
        QCOMPARE(d.kdeMajor, 0);
    }
    void kdePlasma5EmptySessionDesktop() {
        const auto d = Detect("KDE", {}, {}, "5");
        QCOMPARE(d.family, LinuxDesktopFamily::Kde);
        QCOMPARE(d.kdeMajor, 5);
    }
    void gnomeFamily() {
        auto d = Detect("ubuntu:GNOME");
        QCOMPARE(d.family, LinuxDesktopFamily::Gnome);
        QVERIFY(d.usesGnomeProxySettings);
        d = Detect("Budgie:GNOME");
        QCOMPARE(d.family, LinuxDesktopFamily::Gnome);
        QVERIFY(d.usesGnomeProxySettings);
    }
    void cinnamonMateUnityPantheon() {
        auto d = Detect("X-Cinnamon");
        QCOMPARE(d.family, LinuxDesktopFamily::Cinnamon);
        QVERIFY(d.usesGnomeProxySettings);
        d = Detect("MATE");
        QCOMPARE(d.family, LinuxDesktopFamily::Mate);
        QVERIFY(d.usesGnomeProxySettings);
        QVERIFY(Detect("Unity").usesGnomeProxySettings);
        QVERIFY(Detect("Pantheon").usesGnomeProxySettings);
    }
    void xfceOtherUnknown() {
        auto d = Detect("XFCE");
        QCOMPARE(d.family, LinuxDesktopFamily::Xfce);
        QVERIFY(!d.usesGnomeProxySettings);
        d = Detect("sway");
        QCOMPARE(d.family, LinuxDesktopFamily::Other);
        QVERIFY(d.label.contains("sway"));
        QCOMPARE(Detect({}).family, LinuxDesktopFamily::Unknown);
    }
    void caseInsensitiveAndTokenWise() {
        QCOMPARE(Detect("kde").family, LinuxDesktopFamily::Kde);
        QCOMPARE(Detect("KDEConnect-Test").family, LinuxDesktopFamily::Other);
        QCOMPARE(Detect("xfce4-thing").family, LinuxDesktopFamily::Other);
    }
    void invalidKdeVersionIsZero() {
        QCOMPARE(Detect("KDE", {}, {}, "4").kdeMajor, 0);
        QCOMPARE(Detect("KDE", {}, {}, "abc").kdeMajor, 0);
    }
    static LinuxDesktopInfo DetectC(const QString &cur, const QString &sess, const QString &hypr, const QString &niri, const QString &sway) {
        LinuxDesktopEnv env;
        env.currentDesktop = cur;
        env.sessionDesktop = sess;
        env.hyprlandInstance = hypr;
        env.niriSocket = niri;
        env.swaySock = sway;
        return DetectLinuxDesktop(env);
    }

    static void RestoreEnv(const char *name, const QByteArray &old, bool wasSet) {
        if (wasSet) qputenv(name, old);
        else qunsetenv(name);
    }

private slots:
    void compositorDetection_data() {
        QTest::addColumn<QString>("cur");
        QTest::addColumn<QString>("sess");
        QTest::addColumn<QString>("hyprSig");
        QTest::addColumn<QString>("niriSock");
        QTest::addColumn<QString>("swaySock");
        QTest::addColumn<int>("compositor");
        QTest::addColumn<int>("family");
        using C = LinuxCompositor;
        using F = LinuxDesktopFamily;
        const QString hs = "abc_123", ns = "/run/user/1000/niri.wayland-1.1.sock", ss = "/run/user/1000/sway-ipc.1000.1.sock";
        auto row = [](const char *n, const QString &cur, const QString &sess, const QString &h, const QString &nr, const QString &sw, C c, F f) {
            QTest::newRow(n) << cur << sess << h << nr << sw << int(c) << int(f);
        };
        row("Hyprland", "Hyprland", {}, {}, {}, {}, C::Hyprland, F::Other);
        row("hyprland lower", "hyprland", {}, {}, {}, {}, C::Hyprland, F::Other);
        row("sway", "sway", {}, {}, {}, {}, C::Sway, F::Other);
        row("niri", "niri", {}, {}, {}, {}, C::Niri, F::Other);
        row("river", "river", {}, {}, {}, {}, C::River, F::Other);
        row("Wayfire", "Wayfire", {}, {}, {}, {}, C::Wayfire, F::Other);
        row("labwc:wlroots", "labwc:wlroots", {}, {}, {}, {}, C::Labwc, F::Other);
        row("session only", {}, "Hyprland", {}, {}, {}, C::Hyprland, F::Other);
        row("hypr signature", {}, {}, hs, {}, {}, C::Hyprland, F::Unknown);
        row("sway socket", {}, {}, {}, {}, ss, C::Sway, F::Unknown);
        row("niri socket", {}, {}, {}, ns, {}, C::Niri, F::Unknown);
        row("token beats socket", "river", {}, {}, {}, ss, C::River, F::Other);
        row("kde beats socket", "KDE", {}, {}, {}, ss, C::None, F::Kde);
        row("gnome beats signature", "ubuntu:GNOME", {}, hs, {}, {}, C::None, F::Gnome);
        row("xfce", "XFCE", {}, {}, {}, {}, C::None, F::Xfce);
        row("empty", {}, {}, {}, {}, {}, C::None, F::Unknown);
        row("wlroots alone", "wlroots", {}, {}, {}, {}, C::None, F::Other);
        row("swayfx", "swayfx", {}, {}, {}, {}, C::None, F::Other);
        row("sway both", "sway", "sway", {}, {}, {}, C::Sway, F::Other);
    }
    void compositorDetection() {
        QFETCH(QString, cur);
        QFETCH(QString, sess);
        QFETCH(QString, hyprSig);
        QFETCH(QString, niriSock);
        QFETCH(QString, swaySock);
        QFETCH(int, compositor);
        QFETCH(int, family);
        const auto d = DetectC(cur, sess, hyprSig, niriSock, swaySock);
        QCOMPARE(int(d.compositor), compositor);
        QCOMPARE(int(d.family), family);
    }
    void compositorDoesNotChangeTheDesktopAnswer() {
        const QStringList curs{"sway", "Hyprland", "niri", "labwc:wlroots"};
        for (const auto &cur: curs) {
            const auto with = DetectC(cur, {}, "abc", "/n.sock", "/s.sock");
            const auto without = DetectC(cur, {}, {}, {}, {});
            QCOMPARE(with.family, without.family);
            QCOMPARE(with.label, without.label);
            QCOMPARE(with.kdeMajor, without.kdeMajor);
            QCOMPARE(with.usesGnomeProxySettings, without.usesGnomeProxySettings);
        }
        QVERIFY(DetectC("sway", {}, {}, {}, {}).label.contains("sway"));
    }
    void compositorNames() {
        QCOMPARE(LinuxCompositorName(LinuxCompositor::Hyprland), QString("hyprland"));
        QCOMPARE(LinuxCompositorName(LinuxCompositor::Sway), QString("sway"));
        QCOMPARE(LinuxCompositorName(LinuxCompositor::Niri), QString("niri"));
        QCOMPARE(LinuxCompositorName(LinuxCompositor::River), QString("river"));
        QCOMPARE(LinuxCompositorName(LinuxCompositor::Wayfire), QString("wayfire"));
        QCOMPARE(LinuxCompositorName(LinuxCompositor::Labwc), QString("labwc"));
        QCOMPARE(LinuxCompositorName(LinuxCompositor::None), QString());
    }
    void fromProcessReadsCompositorVariables() {
        const bool h = qEnvironmentVariableIsSet("HYPRLAND_INSTANCE_SIGNATURE");
        const bool n = qEnvironmentVariableIsSet("NIRI_SOCKET");
        const bool s = qEnvironmentVariableIsSet("SWAYSOCK");
        const QByteArray ho = qgetenv("HYPRLAND_INSTANCE_SIGNATURE"), no = qgetenv("NIRI_SOCKET"), so = qgetenv("SWAYSOCK");
        qputenv("HYPRLAND_INSTANCE_SIGNATURE", "sig_1");
        qputenv("NIRI_SOCKET", "/run/niri.sock");
        qputenv("SWAYSOCK", "/run/sway.sock");
        const auto env = LinuxDesktopEnvFromProcess();
        RestoreEnv("HYPRLAND_INSTANCE_SIGNATURE", ho, h);
        RestoreEnv("NIRI_SOCKET", no, n);
        RestoreEnv("SWAYSOCK", so, s);
        QCOMPARE(env.hyprlandInstance, QString("sig_1"));
        QCOMPARE(env.niriSocket, QString("/run/niri.sock"));
        QCOMPARE(env.swaySock, QString("/run/sway.sock"));
    }
};

QTEST_APPLESS_MAIN(LinuxDesktopTest)
#include "linux_desktop_test.moc"
