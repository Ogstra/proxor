#include "LinuxSystemProxyPlan.hpp"

#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <utility>

namespace ProxorPlatform {

namespace {

QMutex &ProblemMutex() {
    static QMutex m;
    return m;
}

QString &ProblemText() {
    static QString s;
    return s;
}

QStringList KdeFileArgs(const QString &configDir, const QString &key, const QString &value) {
    return {"--file", configDir + "/kioslaverc", "--group", "Proxy Settings", "--key", key, value};
}

void AppendKdeReparse(QList<ProxyAction> &actions) {
    // KF5 KIO listens for reparseSlaveConfiguration. KF6 renamed slaves to workers internally; the KF6 signal
    // name could not be verified offline, so both are sent. Both are optional: an unknown signal is harmless.
    for (const auto &signal: {QStringLiteral("org.kde.KIO.Scheduler.reparseSlaveConfiguration"),
                              QStringLiteral("org.kde.KIO.Scheduler.reparseWorkerConfiguration")}) {
        actions.append({"dbus-send", {"--type=signal", "/KIO/Scheduler", signal, "string:''"}, false});
    }
}

QString NoKdeToolReason() {
    return QStringLiteral("neither kwriteconfig6 nor kwriteconfig5 is installed");
}

} // namespace

QString FindKdeConfigTool(const LinuxDesktopInfo &desktop, const ProgramLookup &lookup) {
    const auto k6 = lookup(QStringLiteral("kwriteconfig6"));
    const auto k5 = lookup(QStringLiteral("kwriteconfig5"));
    if (desktop.kdeMajor == 5) return !k5.isEmpty() ? k5 : k6;
    // Plasma 6 and unknown version: prefer 6.
    return !k6.isEmpty() ? k6 : k5;
}

LinuxProxyPlan PlanLinuxProxySet(const LinuxDesktopInfo &desktop, const ProgramLookup &lookup, const QString &configDir,
                                 const QString &address, int httpPort, int socksPort) {
    LinuxProxyPlan plan;
    const bool hasHTTP = httpPort > 0 && httpPort < 65536;
    const bool hasSOCKS = socksPort > 0 && socksPort < 65536;
    const bool isKde = desktop.family == LinuxDesktopFamily::Kde;
    const bool isGnome = desktop.usesGnomeProxySettings;
    if (!isKde && !isGnome) {
        plan.unsupportedReason = QStringLiteral("System proxy is unsupported on this Linux desktop: %1").arg(desktop.label);
        return plan;
    }
    if (!hasHTTP && !hasSOCKS) {
        plan.unsupportedReason = QStringLiteral("No proxy port to set");
        return plan;
    }
    QString kdeTool;
    if (isKde) {
        kdeTool = FindKdeConfigTool(desktop, lookup);
        if (kdeTool.isEmpty()) {
            plan.unsupportedReason = QStringLiteral("No KDE proxy configuration tool found: %1").arg(NoKdeToolReason());
            return plan;
        }
    }
    const QString http = QString::number(httpPort);
    const QString socks = QString::number(socksPort);

    if (hasHTTP) {
        for (const auto &protocol: QStringList{"http", "ftp", "https"}) {
            if (isGnome) {
                plan.actions.append({"gsettings", {"set", "org.gnome.system.proxy." + protocol, "host", address}, true});
                plan.actions.append({"gsettings", {"set", "org.gnome.system.proxy." + protocol, "port", http}, true});
            }
            if (isKde) {
                plan.actions.append({kdeTool, KdeFileArgs(configDir, protocol + "Proxy", "http://" + address + " " + http), true});
            }
        }
    }
    if (hasSOCKS) {
        if (isGnome) {
            plan.actions.append({"gsettings", {"set", "org.gnome.system.proxy.socks", "host", address}, true});
            plan.actions.append({"gsettings", {"set", "org.gnome.system.proxy.socks", "port", socks}, true});
        }
        if (isKde) {
            plan.actions.append({kdeTool, KdeFileArgs(configDir, "socksProxy", "socks://" + address + " " + socks), true});
        }
    }
    if (isGnome) plan.actions.append({"gsettings", {"set", "org.gnome.system.proxy", "mode", "manual"}, true});
    if (isKde) {
        plan.actions.append({kdeTool, KdeFileArgs(configDir, "ProxyType", "1"), true});
        AppendKdeReparse(plan.actions);
    }
    return plan;
}

LinuxProxyPlan PlanLinuxProxyClear(const LinuxDesktopInfo &desktop, const ProgramLookup &lookup, const QString &configDir) {
    LinuxProxyPlan plan;
    const bool isKde = desktop.family == LinuxDesktopFamily::Kde;
    if (isKde) {
        const auto tool = FindKdeConfigTool(desktop, lookup);
        if (tool.isEmpty()) {
            plan.unsupportedReason = QStringLiteral("%1: %2").arg(desktop.label, NoKdeToolReason());
            return plan;
        }
        plan.actions.append({tool, KdeFileArgs(configDir, "ProxyType", "0"), true});
        AppendKdeReparse(plan.actions);
        return plan;
    }
    // GNOME family: required. Anything else: tried as before, never fatal.
    plan.actions.append({"gsettings", {"set", "org.gnome.system.proxy", "mode", "none"}, desktop.usesGnomeProxySettings});
    return plan;
}

ProxyRunResult RunLinuxProxyActions(const LinuxProxyPlan &plan, const ProgramRunner &runner) {
    ProxyRunResult result;
    // No short-circuit: every action runs so a partial failure still resets as much as possible.
    for (const auto &action: plan.actions) {
        const int code = runner(action.program, action.arguments);
        if (code == 0) continue;
        if (!action.required) continue;
        result.ok = false;
        const auto cmd = action.program + " " + action.arguments.join(' ');
        if (code == -2) {
            result.failures << QStringLiteral("%1: not found").arg(cmd);
        } else {
            result.failures << QStringLiteral("%1: exit %2").arg(cmd).arg(code);
        }
    }
    return result;
}

int RunProgramBlocking(const QString &program, const QStringList &arguments) {
    // QProcess::execute: -2 not started, -1 crashed, otherwise the exit code.
    return QProcess::execute(program, arguments);
}

void ReportSystemProxyProblem(const QString &problem) {
    QMutexLocker lock(&ProblemMutex());
    ProblemText() = problem;
}

QString TakeSystemProxyProblem() {
    QMutexLocker lock(&ProblemMutex());
    return std::exchange(ProblemText(), QString());
}

} // namespace ProxorPlatform
