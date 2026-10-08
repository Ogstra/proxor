// Linux System Proxy set/clear. Replaces the Linux half of 3rdparty/qv2ray QvProxyConfigurator.cpp
// (cmake/linux/linux.cmake PLATFORM_REPLACED_SOURCES) so Set and Clear share one desktop detection and one KDE tool lookup.
#include "3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.hpp"

#include "platform/LinuxDesktop.hpp"
#include "platform/LinuxSystemProxyPlan.hpp"
#include "sys/LogFile.hpp"

#include <QStandardPaths>

using namespace ProxorPlatform;

namespace {

void LogInfo(const QString &line) {
    ProxorGui_log::Write(ProxorGui_log::Level::Info, "[System Proxy] " + line);
}

void LogPlan(const LinuxProxyPlan &plan) {
    for (const auto &a: plan.actions) {
        LogInfo(QStringLiteral("run: %1 %2%3").arg(a.program, a.arguments.join(' '), a.required ? QString() : QStringLiteral(" (optional)")));
    }
}

QString Lookup(const QString &name) {
    return QStandardPaths::findExecutable(name);
}

QString ClearHint(const LinuxDesktopInfo &desktop) {
    if (desktop.family == LinuxDesktopFamily::Kde) {
        return QStringLiteral("KDE apps may still use 127.0.0.1. Turn the proxy off in System Settings > Network > Proxy.");
    }
    return QStringLiteral("Apps may still use 127.0.0.1. Turn the proxy off in Settings > Network > Network Proxy.");
}

} // namespace

namespace Qv2ray::components::proxy {

    bool SetSystemProxy(int httpPort, int socksPort) {
        const QString address = "127.0.0.1";
        const auto desktop = DetectLinuxDesktop(LinuxDesktopEnvFromProcess());
        const auto configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        const auto plan = PlanLinuxProxySet(desktop, Lookup, configDir, address, httpPort, socksPort);
        if (!plan.unsupportedReason.isEmpty()) {
            LogInfo(plan.unsupportedReason);
            return false;
        }
        LogInfo("Setting System Proxy on " + desktop.label);
        LogPlan(plan);
        const auto result = RunLinuxProxyActions(plan, RunProgramBlocking);
        if (!result.ok) {
            // No problem-sink entry: the caller returns early after a failed set, and a stale entry would later be shown as a clear failure.
            LogInfo("System Proxy could not be set: " + result.failures.join("; "));
            return false;
        }
        return true;
    }

    void ClearSystemProxy() {
        TakeSystemProxyProblem(); // discard anything stale
        const auto desktop = DetectLinuxDesktop(LinuxDesktopEnvFromProcess());
        const auto configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        const auto plan = PlanLinuxProxyClear(desktop, Lookup, configDir);
        if (!plan.unsupportedReason.isEmpty()) {
            const auto problem = QStringLiteral("System Proxy could not be turned off on %1: neither kwriteconfig6 nor kwriteconfig5 is installed. %2")
                                     .arg(desktop.label, ClearHint(desktop));
            LogInfo(problem);
            ReportSystemProxyProblem(problem);
            return;
        }
        LogInfo("Clearing System Proxy on " + desktop.label);
        LogPlan(plan);
        const auto result = RunLinuxProxyActions(plan, RunProgramBlocking);
        if (!result.ok) {
            const auto problem = QStringLiteral("System Proxy could not be turned off on %1: %2. %3")
                                     .arg(desktop.label, result.failures.join("; "), ClearHint(desktop));
            LogInfo(problem);
            ReportSystemProxyProblem(problem);
        }
    }

} // namespace Qv2ray::components::proxy
