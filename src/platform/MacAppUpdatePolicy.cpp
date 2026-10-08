#include "platform/MacAppUpdatePolicy.hpp"

namespace ProxorPlatform {

MacAppUpdateCapabilities MacAppUpdateBuildCapabilities() { return {false, false, false}; }

MacAppUpdateRoute DecideMacAppUpdate(const MacAppUpdateProbe &, const MacAppUpdateCapabilities &) {
    return MacAppUpdateRoute::Guidance;
}

bool IsTranslocatedBundlePath(const QString &) { return false; }
QString MacAppUpdateStageDir(const QString &) { return {}; }
QStringList MacAppUpdateRelaunchArgs(const QStringList &) { return {}; }
QStringList MacAppUpdateInstallArgs(const QString &, qint64, const QString &, const QString &, const QString &,
                                    const QStringList &) {
    return {};
}
QStringList MacAppUpdateRevealArgs(const QString &, const QString &, const QString &, const QString &) { return {}; }
MacAppUpdateResult ParseMacAppUpdateResult(const QString &) { return {}; }

} // namespace ProxorPlatform
