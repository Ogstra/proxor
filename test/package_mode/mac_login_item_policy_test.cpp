#include <QtTest>

#include "platform/MacLoginItemPolicy.hpp"

using namespace ProxorPlatform;

class MacLoginItemPolicyTest : public QObject {
    Q_OBJECT

    static MacLaunchAgentSpec spec(const QString &app = "/Applications/Proxor.app", const QString &dir = "") {
        return DefaultMacLaunchAgentSpec(app, dir);
    }

private slots:
    void plistEqualsGoldenFixture() {
        QFile f(PROXOR_MAC_AGENT_FIXTURE);
        QVERIFY(f.open(QIODevice::ReadOnly));
        // A Windows checkout turns the fixture into CRLF (text=auto); the plist is always LF.
        const QByteArray golden = f.readAll().replace("\r\n", "\n");
        QCOMPARE(MacLaunchAgentPlist(spec()), golden);
    }

    void labelAndBundleId() {
        QCOMPARE(MacAutostartLabel(), QString("io.github.Ogstra.Proxor.autostart"));
        QCOMPARE(spec().bundleId, QString("io.github.Ogstra.Proxor"));
        QCOMPARE(spec().label, MacAutostartLabel());
    }

    void argumentsStartTheAppInTheTray() {
        QCOMPARE(MacLaunchAgentArguments(spec()),
                 (QStringList{"/usr/bin/open", "-a", "/Applications/Proxor.app", "--args", "-tray"}));
    }

    void customAppdataIsAppended() {
        const auto args = MacLaunchAgentArguments(spec("/Applications/Proxor.app", "/Users/u/my cfg"));
        QCOMPARE(args.mid(args.size() - 2), (QStringList{"-appdata", "/Users/u/my cfg"}));
        QCOMPARE(args.size(), 7);
    }

    void parseRoundTripsHardPaths() {
        for (const QString &app : {QString("/Applications/Proxor.app"), QString("/Users/a b/Proxy & <Co>/Proxor.app"),
                                   QString("/Users/u/\"q\"/Proxor.app")}) {
            const auto s = spec(app, "/tmp/a&b <c>");
            QCOMPARE(ParseMacLaunchAgentArguments(MacLaunchAgentPlist(s)), MacLaunchAgentArguments(s));
        }
    }

    void parseRejectsBrokenInput() {
        QCOMPARE(ParseMacLaunchAgentArguments("not xml"), QStringList());
        QCOMPARE(ParseMacLaunchAgentArguments(""), QStringList());
        const QByteArray noArgs =
            "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>Label</key><string>x</string></dict></plist>";
        QCOMPARE(ParseMacLaunchAgentArguments(noArgs), QStringList());
        const QByteArray empty = "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>ProgramArguments</key>"
                                 "<array></array></dict></plist>";
        QCOMPARE(ParseMacLaunchAgentArguments(empty), QStringList());
    }

    void targetIsTheValueAfterDashA() {
        QCOMPARE(MacLaunchAgentTarget(MacLaunchAgentArguments(spec())), QString("/Applications/Proxor.app"));
        QCOMPARE(MacLaunchAgentTarget({"/usr/bin/open", "x"}), QString());
        QCOMPARE(MacLaunchAgentTarget({"/usr/bin/open", "-a"}), QString());
        QCOMPARE(MacLaunchAgentTarget({}), QString());
    }

    void agentFileLivesInTheLaunchAgentsDir() {
        QCOMPARE(MacLaunchAgentFile("/Users/u/Library/LaunchAgents", "io.github.Ogstra.Proxor.autostart"),
                 QString("/Users/u/Library/LaunchAgents/io.github.Ogstra.Proxor.autostart.plist"));
    }

    void refreshOnlyWhenTheOldTargetIsGone() {
        const QStringList expected{"/usr/bin/open", "-a", "/Applications/Proxor.app", "--args", "-tray"};
        const QStringList old{"/usr/bin/open", "-a", "/Volumes/gone/Proxor.app", "--args", "-tray"};
        QVERIFY(ShouldRefreshMacLaunchAgent(old, expected, false));
        QVERIFY(!ShouldRefreshMacLaunchAgent(old, expected, true));
        QVERIFY(!ShouldRefreshMacLaunchAgent(expected, expected, false));
        QVERIFY(!ShouldRefreshMacLaunchAgent({}, expected, false));
    }

    void statusMapping() {
        QCOMPARE(MapSMAppServiceStatus(0), MacLoginItemStatus::NotRegistered);
        QCOMPARE(MapSMAppServiceStatus(1), MacLoginItemStatus::Enabled);
        QCOMPARE(MapSMAppServiceStatus(2), MacLoginItemStatus::RequiresApproval);
        QCOMPARE(MapSMAppServiceStatus(3), MacLoginItemStatus::NotFound);
        QCOMPARE(MapSMAppServiceStatus(7), MacLoginItemStatus::Unknown);
        QCOMPARE(MapSMAppServiceStatus(-1), MacLoginItemStatus::Unknown);
    }

    void viewTable() {
        const QString me = "/Applications/Proxor.app";
        const QString where = "System Settings > General > Login Items & Extensions";
        QCOMPARE(MacLoginItemsLocation(), where);

        // No agent: unchecked, quiet, whatever the status says.
        for (auto st : {MacLoginItemStatus::NotRegistered, MacLoginItemStatus::Enabled, MacLoginItemStatus::RequiresApproval}) {
            const auto v = DecideMacAutostartView(false, "", me, st);
            QVERIFY(!v.checked);
            QVERIFY(!v.needsAttention);
            QVERIFY(v.note.isEmpty());
        }

        // Another copy.
        const auto other = DecideMacAutostartView(true, "/Users/u/dev/Proxor.app", me, MacLoginItemStatus::Enabled);
        QVERIFY(!other.checked);
        QVERIFY(!other.needsAttention);
        QCOMPARE(other.note, QString("Start with system is set for another copy of Proxor (/Users/u/dev/Proxor.app). "
                                     "Turn it on here to start this copy instead."));

        // This copy, Enabled and Unknown: plain checked.
        for (auto st : {MacLoginItemStatus::Enabled, MacLoginItemStatus::Unknown}) {
            const auto v = DecideMacAutostartView(true, me, me, st);
            QVERIFY(v.checked);
            QVERIFY(!v.needsAttention);
            QVERIFY(v.note.isEmpty());
        }
        // A trailing slash is the same app.
        QVERIFY(DecideMacAutostartView(true, me + "/", me, MacLoginItemStatus::Enabled).checked);

        // Turned off in System Settings.
        const auto off = DecideMacAutostartView(true, me, me, MacLoginItemStatus::RequiresApproval);
        QVERIFY(off.checked);
        QVERIFY(off.needsAttention);
        QCOMPARE(off.note, QString("Start with system is turned off in " + where + "."));

        // The file is there but macOS does not list it.
        for (auto st : {MacLoginItemStatus::NotRegistered, MacLoginItemStatus::NotFound}) {
            const auto v = DecideMacAutostartView(true, me, me, st);
            QVERIFY(v.checked);
            QVERIFY(v.needsAttention);
            QCOMPARE(v.note, QString("macOS does not list the Proxor login item yet."));
        }
    }
};

QTEST_APPLESS_MAIN(MacLoginItemPolicyTest)
#include "mac_login_item_policy_test.moc"
