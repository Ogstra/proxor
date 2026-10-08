#include "MacHelperPolicy.h"

#include <QCoreApplication>

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("MacHelperPolicy", text);
}

constexpr int kMaxReasonLength = 300;

} // namespace

MacHelperState ClassifyMacHelper(const MacHelperProbe &probe) {
    // Either file missing (or both) is a broken/absent install: reinstall from scratch.
    if (!probe.plistPresent || !probe.binaryPresent) return MacHelperState::NotInstalled;
    if (!probe.connected) return MacHelperState::InstalledNotRunning;
    // The helper accepted the socket and hung up without a word: peer uid is not allowlisted.
    if (probe.rejected) return MacHelperState::NotAuthorized;
    if (probe.helloOk) {
        return probe.protocol == kMacHelperProtocolVersion ? MacHelperState::Ready : MacHelperState::Outdated;
    }
    if (probe.error.contains(QStringLiteral("protocol mismatch"), Qt::CaseInsensitive)) return MacHelperState::Outdated;
    // A reply that carries a different protocol number.
    if (probe.protocol != 0 && probe.protocol != kMacHelperProtocolVersion) return MacHelperState::Outdated;
    // Connected but nothing usable came back within the timeout: hung helper.
    return MacHelperState::InstalledNotRunning;
}

MacTunStartupDecision DecideMacTunStartup(bool rememberedTun, MacHelperState state) {
    if (!rememberedTun) return {MacTunStartupAction::NoTun, false, QString()};
    switch (state) {
    case MacHelperState::Ready:
        // The only state in which startup may block on Tun (same as Windows/Linux).
        return {MacTunStartupAction::RestoreTun, true, QString()};
    case MacHelperState::NotInstalled:
        return {MacTunStartupAction::SkipNotInstalled, false,
                tr("Tun Mode is remembered, but the Proxor Tun service is not installed (Homebrew upgrades remove it). "
                   "Connecting without Tun; turn on Tun Mode to install the service.")};
    case MacHelperState::InstalledNotRunning:
        return {MacTunStartupAction::SkipNotRunning, false,
                tr("Tun Mode is remembered, but the Proxor Tun service is not running. "
                   "Allow Proxor in System Settings > General > Login Items & Extensions, "
                   "or reinstall it from Tun settings. Connecting without Tun.")};
    case MacHelperState::NotAuthorized:
        return {MacTunStartupAction::SkipNotAuthorized, false,
                tr("Tun Mode is remembered, but the Proxor Tun service on this Mac was installed by another user "
                   "and does not accept this one yet. Connecting without Tun; turn on Tun Mode to allow this user "
                   "(one administrator password prompt).")};
    case MacHelperState::Outdated:
        return {MacTunStartupAction::SkipOutdated, false,
                tr("Tun Mode is remembered, but the Proxor Tun service needs an update. "
                   "Connecting without Tun; turn on Tun Mode to update it.")};
    }
    return {MacTunStartupAction::NoTun, false, QString()};
}

MacHelperEnableAction DecideMacHelperEnable(MacHelperState state) {
    switch (state) {
    case MacHelperState::Ready: return MacHelperEnableAction::Proceed;
    case MacHelperState::NotInstalled: return MacHelperEnableAction::AskInstall;
    case MacHelperState::Outdated: return MacHelperEnableAction::AskUpdate;
    // Reinstalling also appends this uid to the helper's allowlist.
    case MacHelperState::InstalledNotRunning:
    case MacHelperState::NotAuthorized: return MacHelperEnableAction::AskReinstall;
    }
    return MacHelperEnableAction::AskReinstall;
}

MacAdminScriptResult ClassifyMacAdminScriptResult(int exitCode, const QString &stderrText) {
    if (exitCode == 0) return {MacAdminScriptOutcome::Ok, QString()};

    if (stderrText.contains(QStringLiteral("(-128)")) ||
        stderrText.contains(QStringLiteral("User canceled"), Qt::CaseInsensitive)) {
        return {MacAdminScriptOutcome::Cancelled, QString()};
    }

    QString reason;
    const auto lines = stderrText.split(QLatin1Char('\n'));
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const auto line = it->trimmed();
        if (!line.isEmpty()) {
            reason = line;
            break;
        }
    }
    if (reason.isEmpty()) {
        reason = exitCode < 0 ? tr("the administrator script did not finish")
                              : tr("the administrator script failed (exit code %1)").arg(exitCode);
    }
    if (reason.size() > kMaxReasonLength) reason = reason.left(kMaxReasonLength - 3) + QStringLiteral("...");
    return {MacAdminScriptOutcome::Failed, reason};
}

QString MacTunFailureText(const QString &reason) {
    auto cleaned = reason.trimmed();
    while (cleaned.endsWith(QLatin1Char('.'))) cleaned.chop(1);
    if (cleaned.isEmpty()) cleaned = tr("unknown error");
    return tr("Tun could not start: %1. Turn off Tun Mode to connect without it.").arg(cleaned);
}

QStringList MacDefaultProxyBypass() {
    return {QStringLiteral("127.0.0.1"),      QStringLiteral("localhost"),      QStringLiteral("*.local"),
            QStringLiteral("169.254.0.0/16"), QStringLiteral("10.0.0.0/8"),     QStringLiteral("172.16.0.0/12"),
            QStringLiteral("192.168.0.0/16"), QStringLiteral("100.64.0.0/10")};
}

MacStartupInstallDecision DecideMacStartupInstall(bool rememberedTun, bool rememberedSystemProxy,
                                                  MacHelperState state, bool promptedThisSession) {
    const MacStartupInstallDecision none{MacStartupInstallAction::None, MacHelperEnableAction::Proceed, QString(),
                                         QString()};
    if (!rememberedTun && !rememberedSystemProxy) return none;
    if (state == MacHelperState::Ready) return none;

    // Reuse the existing Tun wording when Tun is remembered; System Proxy alone gets its own line.
    const auto logLineFor = [&]() -> QString {
        if (rememberedTun) return DecideMacTunStartup(true, state).logLine;
        switch (state) {
        case MacHelperState::NotInstalled:
            return tr("System Proxy is remembered, but the Proxor service is not installed. "
                      "Connecting without System Proxy; turn on System Proxy to install the service.");
        case MacHelperState::InstalledNotRunning:
            return tr("System Proxy is remembered, but the Proxor service is not running. "
                      "Allow Proxor in System Settings > General > Login Items & Extensions, "
                      "or reinstall it from Tun settings. Connecting without System Proxy.");
        case MacHelperState::NotAuthorized:
            return tr("System Proxy is remembered, but the Proxor service on this Mac was installed by another user "
                      "and does not accept this one yet. Connecting without System Proxy; turn on System Proxy to "
                      "allow this user (one administrator password prompt).");
        case MacHelperState::Outdated:
            return tr("System Proxy is remembered, but the Proxor service needs an update. "
                      "Connecting without System Proxy; turn on System Proxy to update it.");
        case MacHelperState::Ready: break;
        }
        return QString();
    };

    // A reinstall does not fix a Login Items denial: only log.
    if (state == MacHelperState::InstalledNotRunning || promptedThisSession) {
        return {MacStartupInstallAction::LogOnly, MacHelperEnableAction::Proceed, QString(), logLineFor()};
    }

    QString feature;
    if (rememberedTun && rememberedSystemProxy) feature = tr("Tun Mode and System Proxy");
    else if (rememberedTun) feature = tr("Tun Mode");
    else feature = tr("System Proxy");
    return {MacStartupInstallAction::Prompt, DecideMacHelperEnable(state), feature, QString()};
}

QString MacStartupInstallDeclinedText(bool tun, bool systemProxy) {
    if (tun && systemProxy) {
        return tr("Proxor connected without Tun and System Proxy. "
                  "To enable them, turn them on in Settings > Tun settings and the System Proxy option.");
    }
    if (systemProxy) {
        return tr("Proxor connected without System Proxy. "
                  "To enable it, turn on System Proxy again and install the service when asked.");
    }
    return tr("Proxor connected without Tun. To enable it, turn on Tun Mode in Settings > Tun settings.");
}
