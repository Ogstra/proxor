#include "MacHelperPolicy.h"

#include <QtTest>

namespace {

const MacHelperState kAllStates[] = {MacHelperState::NotInstalled, MacHelperState::InstalledNotRunning,
                                     MacHelperState::NotAuthorized, MacHelperState::Outdated,
                                     MacHelperState::Ready};

MacHelperProbe installedProbe() {
    MacHelperProbe p;
    p.plistPresent = true;
    p.binaryPresent = true;
    return p;
}

} // namespace

class MacHelperPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void protocolVersionIsOne();
    void classifyNotInstalledWhenNothingPresent();
    void classifyBrokenInstallIsNotInstalled();
    void classifyInstalledButNoConnectionIsNotRunning();
    void classifyHungHelperIsNotRunning();
    void classifyRightProtocolIsReady();
    void classifyProtocolMismatchIsOutdated();
    void classifyHelloErrorProtocolMismatchIsOutdated();
    void classifyClosedBeforeReplyIsNotAuthorized();

    void noRememberedTunNeverDoesAnything();
    void rememberedTunWithReadyHelperRestoresAndBlocks();
    void pendingIsTrueOnlyForReady();
    void freshConfigRemembersTunButNeverBlocks();
    void skipLogLinesNameTheProblemAndNeverSayDeferred();
    void notAuthorizedLineSaysAnotherUserAndNeverUpdate();

    void enableDecisionPerState();

    void adminScriptOk();
    void adminScriptCancelled();
    void adminScriptFailedKeepsLastNonEmptyLine();
    void adminScriptFailedTruncatesAndHandlesCrash();

    void tunFailureTextNamesReasonAndWayOut();
    void defaultBypassList();

    void startupInstallNothingRememberedIsNone();
    void startupInstallReadyIsNone();
    void startupInstallPromptsWhenServiceMissingOrStale();
    void startupInstallPromptsOncePerSession();
    void startupInstallNotRunningOnlyLogs();
    void startupInstallFeatureNamesWhatIsRemembered();
    void freshConfigPromptsForTun();
    void startupInstallDeclinedText();
    void pauseGraceIsShort();
};

void MacHelperPolicyTest::protocolVersionIsOne() {
    QCOMPARE(kMacHelperProtocolVersion, 1);
}

void MacHelperPolicyTest::classifyNotInstalledWhenNothingPresent() {
    QCOMPARE(ClassifyMacHelper(MacHelperProbe{}), MacHelperState::NotInstalled);
}

void MacHelperPolicyTest::classifyBrokenInstallIsNotInstalled() {
    MacHelperProbe plistOnly;
    plistOnly.plistPresent = true;
    QCOMPARE(ClassifyMacHelper(plistOnly), MacHelperState::NotInstalled);
    MacHelperProbe binaryOnly;
    binaryOnly.binaryPresent = true;
    QCOMPARE(ClassifyMacHelper(binaryOnly), MacHelperState::NotInstalled);
}

void MacHelperPolicyTest::classifyInstalledButNoConnectionIsNotRunning() {
    QCOMPARE(ClassifyMacHelper(installedProbe()), MacHelperState::InstalledNotRunning);
}

void MacHelperPolicyTest::classifyHungHelperIsNotRunning() {
    auto p = installedProbe();
    p.connected = true; // connected, but no reply within the timeout
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::InstalledNotRunning);
}

void MacHelperPolicyTest::classifyRightProtocolIsReady() {
    auto p = installedProbe();
    p.connected = true;
    p.helloOk = true;
    p.protocol = kMacHelperProtocolVersion;
    p.build = QStringLiteral("1.6.12");
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::Ready);
}

void MacHelperPolicyTest::classifyProtocolMismatchIsOutdated() {
    auto p = installedProbe();
    p.connected = true;
    p.helloOk = true;
    p.protocol = kMacHelperProtocolVersion + 1;
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::Outdated);
    p.protocol = 0; // helper predating the protocol field
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::Outdated);
    p.helloOk = false;
    p.protocol = kMacHelperProtocolVersion + 1;
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::Outdated);
}

void MacHelperPolicyTest::classifyHelloErrorProtocolMismatchIsOutdated() {
    auto p = installedProbe();
    p.connected = true;
    p.error = QStringLiteral("protocol mismatch: want 2, got 1");
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::Outdated);
}

void MacHelperPolicyTest::classifyClosedBeforeReplyIsNotAuthorized() {
    auto p = installedProbe();
    p.connected = true;
    p.rejected = true;
    QCOMPARE(ClassifyMacHelper(p), MacHelperState::NotAuthorized);
}

void MacHelperPolicyTest::noRememberedTunNeverDoesAnything() {
    for (const auto state : kAllStates) {
        const auto d = DecideMacTunStartup(false, state);
        QCOMPARE(d.action, MacTunStartupAction::NoTun);
        QVERIFY(!d.setStartupTunPending);
        QVERIFY(d.logLine.isEmpty());
    }
}

void MacHelperPolicyTest::rememberedTunWithReadyHelperRestoresAndBlocks() {
    const auto d = DecideMacTunStartup(true, MacHelperState::Ready);
    QCOMPARE(d.action, MacTunStartupAction::RestoreTun);
    QVERIFY(d.setStartupTunPending);
    QVERIFY(d.logLine.isEmpty());
}

void MacHelperPolicyTest::pendingIsTrueOnlyForReady() {
    for (const auto state : kAllStates) {
        const auto d = DecideMacTunStartup(true, state);
        QCOMPARE(d.setStartupTunPending, state == MacHelperState::Ready);
    }
}

void MacHelperPolicyTest::freshConfigRemembersTunButNeverBlocks() {
    // DataStore::remember_spmode defaults to {"vpn"}, so every new install asks with NotInstalled.
    const auto d = DecideMacTunStartup(true, MacHelperState::NotInstalled);
    QVERIFY(!d.setStartupTunPending);
    QCOMPARE(d.action, MacTunStartupAction::SkipNotInstalled);
    QVERIFY(d.logLine.contains(QStringLiteral("Tun Mode")));
    QVERIFY(d.logLine.contains(QStringLiteral("install")));
}

void MacHelperPolicyTest::skipLogLinesNameTheProblemAndNeverSayDeferred() {
    struct Case {
        MacHelperState state;
        MacTunStartupAction action;
    };
    const Case cases[] = {
        {MacHelperState::NotInstalled, MacTunStartupAction::SkipNotInstalled},
        {MacHelperState::InstalledNotRunning, MacTunStartupAction::SkipNotRunning},
        {MacHelperState::NotAuthorized, MacTunStartupAction::SkipNotAuthorized},
        {MacHelperState::Outdated, MacTunStartupAction::SkipOutdated},
    };
    for (const auto &c : cases) {
        const auto d = DecideMacTunStartup(true, c.state);
        QCOMPARE(d.action, c.action);
        QVERIFY(!d.setStartupTunPending);
        QVERIFY(!d.logLine.isEmpty());
        QVERIFY(d.logLine.contains(QStringLiteral("Tun")));
        QVERIFY(!d.logLine.contains(QStringLiteral("deferred"), Qt::CaseInsensitive));
    }
    QVERIFY(DecideMacTunStartup(true, MacHelperState::InstalledNotRunning)
                .logLine.contains(QStringLiteral("Login Items")));
    QVERIFY(DecideMacTunStartup(true, MacHelperState::Outdated)
                .logLine.contains(QStringLiteral("update"), Qt::CaseInsensitive));
}

void MacHelperPolicyTest::notAuthorizedLineSaysAnotherUserAndNeverUpdate() {
    const auto line = DecideMacTunStartup(true, MacHelperState::NotAuthorized).logLine;
    QVERIFY(line.contains(QStringLiteral("another user")));
    QVERIFY(!line.contains(QStringLiteral("update"), Qt::CaseInsensitive));
}

void MacHelperPolicyTest::enableDecisionPerState() {
    QCOMPARE(DecideMacHelperEnable(MacHelperState::Ready), MacHelperEnableAction::Proceed);
    QCOMPARE(DecideMacHelperEnable(MacHelperState::NotInstalled), MacHelperEnableAction::AskInstall);
    QCOMPARE(DecideMacHelperEnable(MacHelperState::Outdated), MacHelperEnableAction::AskUpdate);
    QCOMPARE(DecideMacHelperEnable(MacHelperState::InstalledNotRunning), MacHelperEnableAction::AskReinstall);
    QCOMPARE(DecideMacHelperEnable(MacHelperState::NotAuthorized), MacHelperEnableAction::AskReinstall);
}

void MacHelperPolicyTest::adminScriptOk() {
    const auto r = ClassifyMacAdminScriptResult(0, QString());
    QCOMPARE(r.outcome, MacAdminScriptOutcome::Ok);
    QCOMPARE(ClassifyMacAdminScriptResult(0, QStringLiteral("some warning")).outcome, MacAdminScriptOutcome::Ok);
}

void MacHelperPolicyTest::adminScriptCancelled() {
    QCOMPARE(ClassifyMacAdminScriptResult(1, QStringLiteral("execution error: User canceled. (-128)")).outcome,
             MacAdminScriptOutcome::Cancelled);
    QCOMPARE(ClassifyMacAdminScriptResult(1, QStringLiteral("0:12: execution error: Anulado (-128)\n")).outcome,
             MacAdminScriptOutcome::Cancelled);
}

void MacHelperPolicyTest::adminScriptFailedKeepsLastNonEmptyLine() {
    const auto r = ClassifyMacAdminScriptResult(1, QStringLiteral("sh: bad source"));
    QCOMPARE(r.outcome, MacAdminScriptOutcome::Failed);
    QCOMPARE(r.reason, QStringLiteral("sh: bad source"));

    const auto r2 = ClassifyMacAdminScriptResult(3, QStringLiteral("first\n  checksum mismatch  \n\n"));
    QCOMPARE(r2.outcome, MacAdminScriptOutcome::Failed);
    QCOMPARE(r2.reason, QStringLiteral("checksum mismatch"));
}

void MacHelperPolicyTest::adminScriptFailedTruncatesAndHandlesCrash() {
    const auto r = ClassifyMacAdminScriptResult(1, QString(1000, QLatin1Char('x')));
    QCOMPARE(r.outcome, MacAdminScriptOutcome::Failed);
    QVERIFY(r.reason.size() <= 300);

    const auto crash = ClassifyMacAdminScriptResult(-1, QString());
    QCOMPARE(crash.outcome, MacAdminScriptOutcome::Failed);
    QVERIFY(!crash.reason.isEmpty());
}

void MacHelperPolicyTest::tunFailureTextNamesReasonAndWayOut() {
    QCOMPARE(MacTunFailureText(QStringLiteral("utun create failed")),
             QStringLiteral("Tun could not start: utun create failed. Turn off Tun Mode to connect without it."));
    QCOMPARE(MacTunFailureText(QString()),
             QStringLiteral("Tun could not start: unknown error. Turn off Tun Mode to connect without it."));
    QVERIFY(MacTunFailureText(QStringLiteral("x")) !=
            QStringLiteral("Profile start is deferred until Tun authorization succeeds."));
}

void MacHelperPolicyTest::defaultBypassList() {
    const QStringList expected{QStringLiteral("127.0.0.1"),    QStringLiteral("localhost"),
                               QStringLiteral("*.local"),      QStringLiteral("169.254.0.0/16"),
                               QStringLiteral("10.0.0.0/8"),   QStringLiteral("172.16.0.0/12"),
                               QStringLiteral("192.168.0.0/16"), QStringLiteral("100.64.0.0/10")};
    QCOMPARE(MacDefaultProxyBypass(), expected);
}

void MacHelperPolicyTest::startupInstallNothingRememberedIsNone() {
    for (const auto state : kAllStates) {
        for (const bool prompted : {false, true}) {
            QCOMPARE(DecideMacStartupInstall(false, false, state, prompted).action, MacStartupInstallAction::None);
        }
    }
}

void MacHelperPolicyTest::startupInstallReadyIsNone() {
    QCOMPARE(DecideMacStartupInstall(true, true, MacHelperState::Ready, false).action, MacStartupInstallAction::None);
    QCOMPARE(DecideMacStartupInstall(true, false, MacHelperState::Ready, true).action, MacStartupInstallAction::None);
    QCOMPARE(DecideMacStartupInstall(false, true, MacHelperState::Ready, false).action, MacStartupInstallAction::None);
}

void MacHelperPolicyTest::startupInstallPromptsWhenServiceMissingOrStale() {
    for (const auto state :
         {MacHelperState::NotInstalled, MacHelperState::Outdated, MacHelperState::NotAuthorized}) {
        const auto d = DecideMacStartupInstall(true, false, state, false);
        QCOMPARE(d.action, MacStartupInstallAction::Prompt);
        QCOMPARE(d.enableAction, DecideMacHelperEnable(state));
        const auto sp = DecideMacStartupInstall(false, true, state, false);
        QCOMPARE(sp.action, MacStartupInstallAction::Prompt);
        QCOMPARE(sp.enableAction, DecideMacHelperEnable(state));
    }
}

void MacHelperPolicyTest::startupInstallPromptsOncePerSession() {
    for (const auto state :
         {MacHelperState::NotInstalled, MacHelperState::Outdated, MacHelperState::NotAuthorized}) {
        const auto d = DecideMacStartupInstall(true, true, state, true);
        QCOMPARE(d.action, MacStartupInstallAction::LogOnly);
        QVERIFY(!d.logLine.isEmpty());
    }
}

void MacHelperPolicyTest::startupInstallNotRunningOnlyLogs() {
    const auto d = DecideMacStartupInstall(true, false, MacHelperState::InstalledNotRunning, false);
    QCOMPARE(d.action, MacStartupInstallAction::LogOnly);
    QCOMPARE(d.logLine, DecideMacTunStartup(true, MacHelperState::InstalledNotRunning).logLine);
    QVERIFY(!d.logLine.isEmpty());
    const auto sp = DecideMacStartupInstall(false, true, MacHelperState::InstalledNotRunning, false);
    QCOMPARE(sp.action, MacStartupInstallAction::LogOnly);
    QVERIFY(!sp.logLine.isEmpty());
}

void MacHelperPolicyTest::startupInstallFeatureNamesWhatIsRemembered() {
    QCOMPARE(DecideMacStartupInstall(true, false, MacHelperState::NotInstalled, false).feature,
             QStringLiteral("Tun Mode"));
    QCOMPARE(DecideMacStartupInstall(false, true, MacHelperState::NotInstalled, false).feature,
             QStringLiteral("System Proxy"));
    QCOMPARE(DecideMacStartupInstall(true, true, MacHelperState::NotInstalled, false).feature,
             QStringLiteral("Tun Mode and System Proxy"));
}

void MacHelperPolicyTest::freshConfigPromptsForTun() {
    // DataStore default remember_spmode is {"vpn"}: a fresh config remembers Tun with no service.
    const auto d = DecideMacStartupInstall(true, false, MacHelperState::NotInstalled, false);
    QCOMPARE(d.action, MacStartupInstallAction::Prompt);
    QCOMPARE(d.feature, QStringLiteral("Tun Mode"));
    QCOMPARE(d.enableAction, MacHelperEnableAction::AskInstall);
}

void MacHelperPolicyTest::startupInstallDeclinedText() {
    const auto tun = MacStartupInstallDeclinedText(true, false);
    QVERIFY(tun.contains(QStringLiteral("without Tun")));
    QVERIFY(tun.contains(QStringLiteral("Settings > Tun settings")));
    QVERIFY(!tun.contains(QStringLiteral("deferred")));
    QVERIFY(!tun.endsWith(QStringLiteral("..")));
    const auto sp = MacStartupInstallDeclinedText(false, true);
    QVERIFY(sp.contains(QStringLiteral("System Proxy")));
    QVERIFY(!sp.contains(QStringLiteral("deferred")));
    QVERIFY(!sp.endsWith(QStringLiteral("..")));
}

void MacHelperPolicyTest::pauseGraceIsShort() {
    QCOMPARE(kMacPauseGraceMs, 750);
}

QTEST_APPLESS_MAIN(MacHelperPolicyTest)
#include "mac_helper_policy_test.moc"
