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
};

QTEST_APPLESS_MAIN(LinuxDesktopTest)
#include "linux_desktop_test.moc"
