#include "MacHelperPolicy.h"

// RED stub: intentionally wrong until the GREEN commit.
MacHelperState ClassifyMacHelper(const MacHelperProbe &) { return MacHelperState::Ready; }
MacTunStartupDecision DecideMacTunStartup(bool, MacHelperState) {
    return {MacTunStartupAction::RestoreTun, true, QString()};
}
MacHelperEnableAction DecideMacHelperEnable(MacHelperState) { return MacHelperEnableAction::Proceed; }
MacAdminScriptResult ClassifyMacAdminScriptResult(int, const QString &) {
    return {MacAdminScriptOutcome::Ok, QString()};
}
QString MacTunFailureText(const QString &) { return QString(); }
QStringList MacDefaultProxyBypass() { return {}; }
