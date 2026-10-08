#pragma once

// macOS "Start with system" as a user LaunchAgent: the plist text, how it is read back, how macOS's own
// login item status maps to the Settings checkbox. Pure Qt Core; compiled on every OS (harmless off macOS)
// so the tests run on all runners.

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace ProxorPlatform {

enum class MacLoginItemStatus { NotRegistered, Enabled, RequiresApproval, NotFound, Unknown };

// SMAppServiceStatus raw values: 0 not registered, 1 enabled, 2 requires approval, 3 not found.
MacLoginItemStatus MapSMAppServiceStatus(long raw);

QString MacAutostartLabel(); // "io.github.Ogstra.Proxor.autostart"

struct MacLaunchAgentSpec {
    QString label;
    QString bundleId;
    QString appBundlePath;
    QString appdataDir; // empty = the default data location
};
MacLaunchAgentSpec DefaultMacLaunchAgentSpec(const QString &appBundlePath, const QString &appdataDir);

// {"/usr/bin/open", "-a", app, "--args", "-tray"} plus {"-appdata", dir} when a custom directory is used.
QStringList MacLaunchAgentArguments(const MacLaunchAgentSpec &spec);
// Exact XML, tab-indented like plutil output, values XML-escaped.
QByteArray MacLaunchAgentPlist(const MacLaunchAgentSpec &spec);
// ProgramArguments strings; empty when missing or invalid.
QStringList ParseMacLaunchAgentArguments(const QByteArray &plistXml);
// The value after "-a", or empty.
QString MacLaunchAgentTarget(const QStringList &arguments);
QString MacLaunchAgentFile(const QString &launchAgentsDir, const QString &label);

// True only when the existing agent is non-empty, differs from the expected one and its target app no longer
// exists. An agent pointing at another EXISTING copy of Proxor is never rewritten.
bool ShouldRefreshMacLaunchAgent(const QStringList &existing, const QStringList &expected, bool existingTargetExists);

struct MacAutostartView {
    bool checked = false;
    bool needsAttention = false;
    QString note;
};
MacAutostartView DecideMacAutostartView(bool agentExists, const QString &agentTarget, const QString &thisApp,
                                        MacLoginItemStatus status);

QString MacLoginItemsLocation(); // "System Settings > General > Login Items & Extensions"

} // namespace ProxorPlatform
