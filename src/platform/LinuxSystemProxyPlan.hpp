#pragma once

#include "LinuxDesktop.hpp"

#include <QList>
#include <QString>
#include <QStringList>
#include <functional>

// Pure planning of the Linux System Proxy set/clear commands (Qt Core only) plus an injectable runner.
namespace ProxorPlatform {

struct ProxyAction {
    QString program;
    QStringList arguments;
    bool required = true;
};

struct LinuxProxyPlan {
    QList<ProxyAction> actions;
    QString unsupportedReason; // non-empty => nothing to run
};

using ProgramLookup = std::function<QString(const QString &name)>; // absolute path or empty

QString FindKdeConfigTool(const LinuxDesktopInfo &desktop, const ProgramLookup &lookup);
LinuxProxyPlan PlanLinuxProxySet(const LinuxDesktopInfo &desktop, const ProgramLookup &lookup, const QString &configDir,
                                 const QString &address, int httpPort, int socksPort);
LinuxProxyPlan PlanLinuxProxyClear(const LinuxDesktopInfo &desktop, const ProgramLookup &lookup, const QString &configDir);

struct ProxyRunResult {
    bool ok = true;
    QStringList failures;
};
using ProgramRunner = std::function<int(const QString &program, const QStringList &arguments)>; // exit code, -2 not started
ProxyRunResult RunLinuxProxyActions(const LinuxProxyPlan &plan, const ProgramRunner &runner);
int RunProgramBlocking(const QString &program, const QStringList &arguments);

// Written by the Linux implementation, read by shared UI code (always empty on Windows/macOS).
void ReportSystemProxyProblem(const QString &problem);
QString TakeSystemProxyProblem();

} // namespace ProxorPlatform
