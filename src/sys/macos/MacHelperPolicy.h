#pragma once

// Pure, platform-neutral decision functions for the macOS privileged-helper lifecycle.
// Qt Core only: no macOS APIs, so the unit test compiles on every CI runner.

#include <QString>
#include <QStringList>

// MUST equal machelper.ProtocolVersion (Go); guarded by test/macos-helper/test-protocol-version.sh.
constexpr int kMacHelperProtocolVersion = 1;

// What a non-blocking probe of the helper observed.
struct MacHelperProbe {
    bool plistPresent = false;
    bool binaryPresent = false;
    bool connected = false;
    bool helloOk = false;
    bool rejected = false; // helper closed the connection before any reply: uid not allowlisted
    int protocol = 0;
    QString build;
    QString singbox;
    QString error;
};

enum class MacHelperState { NotInstalled, InstalledNotRunning, NotAuthorized, Outdated, Ready };

MacHelperState ClassifyMacHelper(const MacHelperProbe &probe);

enum class MacTunStartupAction {
    NoTun,
    RestoreTun,
    SkipNotInstalled,
    SkipNotRunning,
    SkipNotAuthorized,
    SkipOutdated
};

struct MacTunStartupDecision {
    MacTunStartupAction action;
    bool setStartupTunPending;
    QString logLine;
};

// Startup may only become pending (blocked on Tun) when the helper answered with the right
// protocol, i.e. state == Ready. Every other state skips Tun and never blocks a profile start.
MacTunStartupDecision DecideMacTunStartup(bool rememberedTun, MacHelperState state);

enum class MacHelperEnableAction { Proceed, AskInstall, AskUpdate, AskReinstall };

// Manual toggle of Tun or System Proxy.
MacHelperEnableAction DecideMacHelperEnable(MacHelperState state);

enum class MacAdminScriptOutcome { Ok, Cancelled, Failed };

struct MacAdminScriptResult {
    MacAdminScriptOutcome outcome;
    QString reason;
};

// Classifies an `osascript ... with administrator privileges` run from exit code + stderr only.
MacAdminScriptResult ClassifyMacAdminScriptResult(int exitCode, const QString &stderrText);

// "Tun could not start: <reason>. Turn off Tun Mode to connect without it."
QString MacTunFailureText(const QString &reason);

// Same list as Go machelper.DefaultBypass.
QStringList MacDefaultProxyBypass();
