#include <QtTest>

#include "platform/LinuxAutostart.hpp"

using namespace ProxorPlatform;

class AutostartTest : public QObject {
    Q_OBJECT

    static AutostartInputs native(PackageMode mode) {
        AutostartInputs in;
        in.packageMode = mode;
        in.applicationFilePath = "/usr/lib/proxor/proxor";
        in.nativeWrapperPath = "/usr/bin/proxor";
        in.useAppdata = true;
        return in;
    }

private slots:
    void nativeChannelsUseTheWrapper() {
        for (auto mode : {PackageMode::Deb, PackageMode::Rpm, PackageMode::Arch, PackageMode::NativeUnknownManager}) {
            QCOMPARE(LinuxAutostartCommand(native(mode)), (QStringList{"/usr/bin/proxor", "-tray", "-appdata"}));
        }
    }

    void nativeEntryEqualsGoldenFixture() {
        QFile f(PROXOR_AUTOSTART_FIXTURE);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString golden = QString::fromUtf8(f.readAll());
        QCOMPARE(LinuxAutostartDesktopEntry("proxor", LinuxAutostartCommand(native(PackageMode::Deb))), golden);
    }

    void appImageKeepsItsPath() {
        AutostartInputs in;
        in.packageMode = PackageMode::AppImage;
        in.appImagePath = "/home/u/Apps/Proxor.AppImage";
        in.applicationFilePath = "/tmp/.mount_x/usr/bin/proxor";
        QCOMPARE(LinuxAutostartCommand(in), (QStringList{"/home/u/Apps/Proxor.AppImage", "-tray"}));
        in.useAppdata = true;
        QCOMPARE(LinuxAutostartCommand(in), (QStringList{"/home/u/Apps/Proxor.AppImage", "-tray", "-appdata"}));
    }

    void launcherIsUnchanged() {
        auto in = native(PackageMode::Deb);
        in.fromLauncher = true;
        in.launcherPath = "/opt/p/launcher";
        QCOMPARE(LinuxAutostartCommand(in), (QStringList{"/opt/p/launcher", "--", "-tray", "-appdata"}));
    }

    void portableUsesTheBinary() {
        AutostartInputs in;
        in.applicationFilePath = "/opt/p/proxor";
        QCOMPARE(LinuxAutostartCommand(in), (QStringList{"/opt/p/proxor", "-tray"}));
    }

    void customAppdataDirIsKept() {
        auto in = native(PackageMode::Deb);
        in.appdataDir = "/home/u/my cfg";
        const auto cmd = LinuxAutostartCommand(in);
        QCOMPARE(cmd, (QStringList{"/usr/bin/proxor", "-tray", "-appdata", "/home/u/my cfg"}));
        QVERIFY(LinuxAutostartDesktopEntry("proxor", cmd).contains("\"/home/u/my cfg\""));
    }

    void missingWrapperFallsBack() {
        for (auto mode : {PackageMode::Deb, PackageMode::Rpm, PackageMode::Arch, PackageMode::NativeUnknownManager}) {
            auto in = native(mode);
            in.nativeWrapperPath.clear();
            QCOMPARE(LinuxAutostartCommand(in), (QStringList{"/usr/lib/proxor/proxor", "-tray", "-appdata"}));
        }
    }

    void wrapperPathFromPackageRoot() {
        QCOMPARE(NativeWrapperPathFor("/usr/lib/proxor"), QString("/usr/bin/proxor"));
        QCOMPARE(NativeWrapperPathFor("/opt/x/lib/proxor"), QString("/opt/x/bin/proxor"));
    }

    void replaceExecKeepsEverythingElse() {
        const QString text =
            "# comment\n[Desktop Entry]\nName=proxor\nName[de]=Proxor DE\nExec=\"/usr/lib/proxor/proxor\" \"-tray\"\n"
            "Icon=proxor\nX-GNOME-Autostart-Delay=5\n";
        const auto out = ReplaceDesktopEntryExec(text, {"/usr/bin/proxor", "-tray"});
        QCOMPARE(out,
                 QString("# comment\n[Desktop Entry]\nName=proxor\nName[de]=Proxor DE\nExec=\"/usr/bin/proxor\" \"-tray\"\n"
                         "Icon=proxor\nX-GNOME-Autostart-Delay=5\n"));
    }

    void disabledDetection() {
        QVERIFY(IsAutostartEntryDisabled("[Desktop Entry]\nX-GNOME-Autostart-enabled=false\n"));
        QVERIFY(IsAutostartEntryDisabled("[Desktop Entry]\nHidden=true\n"));
        QVERIFY(!IsAutostartEntryDisabled("[Desktop Entry]\nX-GNOME-Autostart-enabled=true\n"));
        QVERIFY(!IsAutostartEntryDisabled("[Desktop Entry]\nHidden=false\n"));
        QVERIFY(!IsAutostartEntryDisabled("[Desktop Entry]\nName=x\n"));
    }

    void parseRoundTrips() {
        const QStringList cmd{"/home/u/my dir/proxor", "-tray", "a\"b", "c\\d"};
        QCOMPARE(ParseDesktopEntryExec(LinuxAutostartDesktopEntry("proxor", cmd)), cmd);
        QVERIFY(ParseDesktopEntryExec("[Desktop Entry]\nName=x\n").isEmpty());
    }

    void refreshDecision() {
        const QStringList expected{"/usr/bin/proxor", "-tray", "-appdata"};
        QVERIFY(ShouldRefreshAutostart({"/usr/lib/proxor/proxor", "-tray"}, expected, "/usr/lib/proxor/proxor"));
        QVERIFY(!ShouldRefreshAutostart(expected, expected, "/usr/lib/proxor/proxor"));
        QVERIFY(!ShouldRefreshAutostart({"/home/u/Proxor.AppImage", "-tray"}, expected, "/usr/lib/proxor/proxor"));
    }
};

QTEST_APPLESS_MAIN(AutostartTest)
#include "autostart_test.moc"
