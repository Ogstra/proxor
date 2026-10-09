#include "platform/MacAppUpdatePolicy.hpp"

#include <QDir>

namespace ProxorPlatform {

namespace {
QString CleanBundle(const QString &bundlePath) {
    if (bundlePath.trimmed().isEmpty()) return {};
    return QDir::cleanPath(bundlePath);
}
} // namespace

MacAppUpdateCapabilities MacAppUpdateBuildCapabilities() {
    MacAppUpdateCapabilities c;
    c.inPlace = true;      // SWAP=rename2 and FALLBACK=none (60-RESEARCH ## Decisions)
    c.reveal = false;      // FALLBACK=none: no reveal route
    c.requireOwner = true; // WRITABLE=parent+owner
    return c;
}

bool IsTranslocatedBundlePath(const QString &bundlePath) {
    return bundlePath.contains(QStringLiteral("/AppTranslocation/"));
}

MacAppUpdateRoute DecideMacAppUpdate(const MacAppUpdateProbe &probe, const MacAppUpdateCapabilities &caps) {
    if (!probe.macApp) return MacAppUpdateRoute::Guidance;
    const QString bundle = CleanBundle(probe.bundlePath);
    if (bundle.isEmpty() || !bundle.endsWith(QStringLiteral(".app")) || IsTranslocatedBundlePath(bundle))
        return MacAppUpdateRoute::Guidance;
    if (!probe.scriptPresent) return MacAppUpdateRoute::Guidance;
    if (caps.inPlace && probe.parentWritable && (!caps.requireOwner || probe.bundleOwnedByUser))
        return MacAppUpdateRoute::InPlace;
    if (caps.reveal) return MacAppUpdateRoute::Reveal;
    return MacAppUpdateRoute::Guidance;
}

QString MacAppUpdateStageDir(const QString &bundlePath) {
    const QString bundle = CleanBundle(bundlePath);
    const int slash = bundle.lastIndexOf('/');
    QString parent;
    if (slash > 0) parent = bundle.left(slash);
    else if (slash == 0) parent = QStringLiteral("/");
    else parent = QStringLiteral(".");
    if (parent.endsWith('/')) return parent + QStringLiteral(".proxor-update");
    return parent + QStringLiteral("/.proxor-update");
}

QStringList MacAppUpdateRelaunchArgs(const QStringList &appArguments) {
    QStringList out;
    for (int i = 1; i < appArguments.size(); ++i) {
        const QString &a = appArguments.at(i);
        if (a == QStringLiteral("-tray") || a == QStringLiteral("-flag_restart_tun_on") ||
            a == QStringLiteral("-flag_reorder") || a.startsWith(QStringLiteral("-psn_")))
            continue;
        out << a;
    }
    return out;
}

QStringList MacAppUpdateInstallArgs(const QString &scriptPath, qint64 pid, const QString &zipPath,
                                    const QString &bundlePath, const QString &resultFile,
                                    const QStringList &relaunchArgs) {
    QStringList out{scriptPath, QStringLiteral("install"), QString::number(pid), zipPath, bundlePath, resultFile,
                    QStringLiteral("--")};
    out << relaunchArgs;
    return out;
}

QStringList MacAppUpdateRevealArgs(const QString &scriptPath, const QString &zipPath, const QString &destDir,
                                   const QString &resultFile) {
    return {scriptPath, QStringLiteral("reveal"), zipPath, destDir, resultFile};
}

MacAppUpdateResult ParseMacAppUpdateResult(const QString &text) {
    MacAppUpdateResult r;
    const QString t = text.trimmed();
    if (t.isEmpty()) return r;
    r.present = true;
    if (t.startsWith(QStringLiteral("ok "))) {
        const QString v = t.mid(3).trimmed();
        if (!v.isEmpty()) {
            r.ok = true;
            r.version = v;
            return r;
        }
    } else if (t.startsWith(QStringLiteral("revealed "))) {
        const QString p = t.mid(9).trimmed();
        if (!p.isEmpty()) {
            r.ok = true;
            r.message = p;
            return r;
        }
    } else if (t.startsWith(QStringLiteral("failed "))) {
        const QString rest = t.mid(7).trimmed();
        const int colon = rest.indexOf(':');
        if (colon > 0) {
            const QString stage = rest.left(colon);
            bool word = true;
            for (const QChar c : stage)
                if (!c.isLetterOrNumber() && c != '_' && c != '-') word = false;
            if (word) {
                r.stage = stage;
                r.message = rest.mid(colon + 1).trimmed();
            }
        }
        if (r.stage.isEmpty()) r.message = rest;
    } else {
        r.message = t;
    }
    if (r.message.isEmpty()) r.message = QStringLiteral("unknown error");
    return r;
}

} // namespace ProxorPlatform
