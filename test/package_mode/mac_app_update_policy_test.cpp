#include <QtTest>

#include "platform/MacAppUpdatePolicy.hpp"

using namespace ProxorPlatform;

class MacAppUpdatePolicyTest : public QObject {
    Q_OBJECT

    static MacAppUpdateProbe good() {
        MacAppUpdateProbe p;
        p.macApp = true;
        p.bundlePath = "/Users/u/Apps/Proxor.app";
        p.parentWritable = true;
        p.bundleOwnedByUser = true;
        p.scriptPresent = true;
        return p;
    }
    static MacAppUpdateCapabilities caps(bool inPlace, bool reveal, bool owner = false) {
        return MacAppUpdateCapabilities{inPlace, reveal, owner};
    }

private slots:
    void guidanceForEveryModeButMacApp() {
        auto p = good();
        p.macApp = false;
        QCOMPARE(DecideMacAppUpdate(p, caps(true, true)), MacAppUpdateRoute::Guidance);
    }
    void inPlaceWhenWritableAndSupported() {
        QCOMPARE(DecideMacAppUpdate(good(), caps(true, false)), MacAppUpdateRoute::InPlace);
    }
    void guidanceWhenParentNotWritable() {
        auto p = good();
        p.parentWritable = false;
        QCOMPARE(DecideMacAppUpdate(p, caps(true, false)), MacAppUpdateRoute::Guidance);
        QCOMPARE(DecideMacAppUpdate(p, caps(true, true)), MacAppUpdateRoute::Reveal);
    }
    void ownerRequired() {
        auto p = good();
        p.bundleOwnedByUser = false;
        QCOMPARE(DecideMacAppUpdate(p, caps(true, false, true)), MacAppUpdateRoute::Guidance);
        QCOMPARE(DecideMacAppUpdate(p, caps(true, true, true)), MacAppUpdateRoute::Reveal);
        QCOMPARE(DecideMacAppUpdate(p, caps(true, false, false)), MacAppUpdateRoute::InPlace);
    }
    void translocatedIsGuidance() {
        const QString t = "/private/var/folders/ab/T/AppTranslocation/1234-ABCD/d/Proxor.app";
        auto p = good();
        p.bundlePath = t;
        QCOMPARE(DecideMacAppUpdate(p, caps(true, false)), MacAppUpdateRoute::Guidance);
        QVERIFY(IsTranslocatedBundlePath(t));
        QVERIFY(!IsTranslocatedBundlePath("/Applications/Proxor.app"));
    }
    void notABundleIsGuidance() {
        auto p = good();
        for (const QString &b : {QString(), QString("/Users/u/Proxor"), QString("/Users/u/Proxor.app.bak")}) {
            p.bundlePath = b;
            QCOMPARE(DecideMacAppUpdate(p, caps(true, false)), MacAppUpdateRoute::Guidance);
        }
        p.bundlePath = "/Users/u/Prox (1).app/";
        QCOMPARE(DecideMacAppUpdate(p, caps(true, false)), MacAppUpdateRoute::InPlace);
    }
    void scriptMissingIsGuidance() {
        auto p = good();
        p.scriptPresent = false;
        QCOMPARE(DecideMacAppUpdate(p, caps(true, true)), MacAppUpdateRoute::Guidance);
    }
    void revealOnlyWhenSupported() {
        QCOMPARE(DecideMacAppUpdate(good(), caps(false, true)), MacAppUpdateRoute::Reveal);
        QCOMPARE(DecideMacAppUpdate(good(), caps(false, false)), MacAppUpdateRoute::Guidance);
    }
    void buildCapabilitiesMatchResearch() {
        // SWAP=rename2, FALLBACK=none, WRITABLE=parent+owner
        const auto c = MacAppUpdateBuildCapabilities();
        QVERIFY(c.inPlace);
        QVERIFY(!c.reveal);
        QVERIFY(c.requireOwner);
    }
    void stageDir() {
        QCOMPARE(MacAppUpdateStageDir("/Applications/Proxor.app"), QString("/Applications/.proxor-update"));
        QCOMPARE(MacAppUpdateStageDir("/Users/u/My Apps/Prox (1).app/"), QString("/Users/u/My Apps/.proxor-update"));
    }
    void relaunchArgs() {
        QCOMPARE(MacAppUpdateRelaunchArgs({"/x/Proxor", "-tray", "-many", "-appdata", "/d", "-flag_restart_tun_on",
                                           "-flag_reorder", "-psn_0_123"}),
                 (QStringList{"-many", "-appdata", "/d"}));
        QCOMPARE(MacAppUpdateRelaunchArgs({}), QStringList());
    }
    void installArgs() {
        QCOMPARE(MacAppUpdateInstallArgs("/s.sh", 4242, "/z.zip", "/A/Proxor.app", "/r.txt", {"-many"}),
                 (QStringList{"/s.sh", "install", "4242", "/z.zip", "/A/Proxor.app", "/r.txt", "--", "-many"}));
        QCOMPARE(MacAppUpdateInstallArgs("/s.sh", 1, "/z.zip", "/A/Proxor.app", "/r.txt", {}),
                 (QStringList{"/s.sh", "install", "1", "/z.zip", "/A/Proxor.app", "/r.txt", "--"}));
    }
    void revealArgs() {
        QCOMPARE(MacAppUpdateRevealArgs("/s.sh", "/z.zip", "/D", "/r.txt"),
                 (QStringList{"/s.sh", "reveal", "/z.zip", "/D", "/r.txt"}));
    }
    void parseResult() {
        auto r = ParseMacAppUpdateResult("");
        QVERIFY(!r.present);

        r = ParseMacAppUpdateResult("ok 1.6.15\n");
        QVERIFY(r.present && r.ok);
        QCOMPARE(r.version, QString("1.6.15"));
        r = ParseMacAppUpdateResult("ok 1.6.15");
        QVERIFY(r.present && r.ok);
        QCOMPARE(r.version, QString("1.6.15"));

        r = ParseMacAppUpdateResult("failed start: the new version did not keep running\n");
        QVERIFY(r.present && !r.ok);
        QCOMPARE(r.stage, QString("start"));
        QCOMPARE(r.message, QString("the new version did not keep running"));

        r = ParseMacAppUpdateResult("failed wait: Proxor did not quit within 60 seconds");
        QCOMPARE(r.stage, QString("wait"));
        QCOMPARE(r.message, QString("Proxor did not quit within 60 seconds"));

        r = ParseMacAppUpdateResult("revealed /Users/u/Downloads/Proxor 1.6.15/Proxor.app");
        QVERIFY(r.present && r.ok);
        QCOMPARE(r.message, QString("/Users/u/Downloads/Proxor 1.6.15/Proxor.app"));

        r = ParseMacAppUpdateResult("garbage");
        QVERIFY(r.present && !r.ok);
        QCOMPARE(r.stage, QString());
        QCOMPARE(r.message, QString("garbage"));

        r = ParseMacAppUpdateResult("failed ");
        QVERIFY(r.present && !r.ok);
        QVERIFY(!r.message.isEmpty());
    }
    void constants() {
        QCOMPARE(QString(kMacAppUpdateScriptFromMacOSDir), QString("../Resources/update/proxor-app-update.sh"));
        QCOMPARE(QString(kMacAppUpdateResultFileName), QString("mac-update-result.txt"));
    }
};

QTEST_APPLESS_MAIN(MacAppUpdatePolicyTest)
#include "mac_app_update_policy_test.moc"
