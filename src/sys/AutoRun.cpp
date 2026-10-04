#include "AutoRun.hpp"

#include <QApplication>
#include <QDir>

#include "3rdparty/fix_old_qt.h"
#include "main/ProxorGui.hpp"

// macOS headers (possibly OBJ-c)
#if defined(Q_OS_MACOS)
#include <QFile>
#include <QFileInfo>

#include "sys/macos/MacLoginItem.h"
#endif

#ifdef Q_OS_WIN

#include <QSettings>

QString Windows_GenAutoRunString() {
    auto appPath = ProxorGui::PackageExecutablePath("proxor");
    appPath = "\"" + QDir::toNativeSeparators(appPath) + "\"";
    appPath += " -tray";
    return appPath;
}

void AutoRun_SetEnabled(bool enable) {
    // 以程序名称作为注册表中的键
    // 根据键获取对应的值（程序路径）
    auto appPath = ProxorGui::PackageExecutablePath("proxor");
    QFileInfo fInfo(appPath);
    QString name = fInfo.baseName();

    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    if (enable) {
        settings.setValue(name, Windows_GenAutoRunString());
    } else {
        settings.remove(name);
    }
}

bool AutoRun_IsEnabled() {
    QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

    // 以程序名称作为注册表中的键
    // 根据键获取对应的值（程序路径）
    auto appPath = ProxorGui::PackageExecutablePath("proxor");
    QFileInfo fInfo(appPath);
    QString name = fInfo.baseName();

    return settings.value(name).toString() == Windows_GenAutoRunString();
}
QString AutoRun_RefreshStaleEntry() { return {}; }

#endif

#ifdef Q_OS_MACOS

// "Start with system" is a user LaunchAgent that runs `open -a <this app> --args -tray` at login; the old
// shared-file-list login item API is unsupported since macOS 10.11 and does nothing on current macOS.
namespace {

QString agentFile() {
    return ProxorPlatform::MacLaunchAgentFile(ProxorMac::LaunchAgentsDir(), ProxorPlatform::MacAutostartLabel());
}

// A custom -appdata directory is carried into the agent; the default location needs no argument.
ProxorPlatform::MacLaunchAgentSpec expectedSpec() {
    return ProxorPlatform::DefaultMacLaunchAgentSpec(ProxorMac::CurrentAppBundlePath(),
                                                     ProxorGui::dataStore->appdataDir);
}

QStringList readExisting() {
    QFile f(agentFile());
    if (!f.open(QIODevice::ReadOnly)) return {};
    return ProxorPlatform::ParseMacLaunchAgentArguments(f.readAll());
}

void reportAgentFailure(const QString &what) {
    const QString text = QObject::tr("Start with system: %1").arg(what);
    if (MW_show_log) MW_show_log(text);
    MessageBoxWarning(software_name, text);
}

} // namespace

void AutoRun_SetEnabled(bool enable) {
    const QString file = agentFile();
    QString error;
    if (enable) {
        if (!ProxorMac::WriteLaunchAgent(file, ProxorPlatform::MacLaunchAgentPlist(expectedSpec()), &error)) {
            reportAgentFailure(QObject::tr("cannot write %1: %2").arg(file, error));
            return;
        }
        if (MW_show_log)
            MW_show_log(QObject::tr("Start with system: Proxor will start in the menu bar when you log in (%1).").arg(file));
        if (ProxorMac::LegacyAgentStatus(file) == ProxorPlatform::MacLoginItemStatus::RequiresApproval) {
            const auto view = ProxorPlatform::DecideMacAutostartView(
                true, ProxorPlatform::MacLaunchAgentTarget(readExisting()), ProxorMac::CurrentAppBundlePath(),
                ProxorPlatform::MacLoginItemStatus::RequiresApproval);
            if (MW_show_log) MW_show_log(view.note);
            ProxorMac::OpenLoginItemsSettings();
        }
    } else {
        if (!ProxorMac::RemoveLaunchAgent(file, ProxorPlatform::MacAutostartLabel(), &error)) {
            reportAgentFailure(QObject::tr("cannot turn off: %1").arg(error));
            return;
        }
        if (MW_show_log) MW_show_log(QObject::tr("Start with system: turned off."));
    }
}

bool AutoRun_IsEnabled() {
    const QString file = agentFile();
    return ProxorPlatform::DecideMacAutostartView(QFile::exists(file),
                                                  ProxorPlatform::MacLaunchAgentTarget(readExisting()),
                                                  ProxorMac::CurrentAppBundlePath(),
                                                  ProxorMac::LegacyAgentStatus(file))
        .checked;
}

QString AutoRun_RefreshStaleEntry() {
    const QString file = agentFile();
    if (!QFile::exists(file)) return {};
    const QStringList existing = readExisting();
    const QStringList expected = ProxorPlatform::MacLaunchAgentArguments(expectedSpec());
    if (!ProxorPlatform::ShouldRefreshMacLaunchAgent(
            existing, expected, QFileInfo::exists(ProxorPlatform::MacLaunchAgentTarget(existing))))
        return {};
    QString error;
    if (!ProxorMac::WriteLaunchAgent(file, ProxorPlatform::MacLaunchAgentPlist(expectedSpec()), &error)) {
        if (MW_show_log) MW_show_log(QObject::tr("Start with system: cannot write %1: %2").arg(file, error));
        return {};
    }
    return "Start with system: updated the login agent to start " + ProxorMac::CurrentAppBundlePath() +
           " (the old one pointed at a Proxor that no longer exists).";
}

#endif

#ifdef Q_OS_LINUX

#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include "platform/LinuxAutostart.hpp"
#include "sys/DesktopPortal.hpp"

//  launchatlogin.cpp
//  ShadowClash
//
//  Created by TheWanderingCoel on 2018/6/12.
//  Copyright © 2019 Coel Wu. All rights reserved.
//
static QString getUserAutostartDir_private() {
    QString config = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    config += QLatin1String("/autostart/");
    return config;
}

static QString autostartFilePath() {
    return getUserAutostartDir_private() + QCoreApplication::applicationName() + QLatin1String(".desktop");
}

static ProxorPlatform::AutostartInputs CurrentAutostartInputs() {
    ProxorPlatform::AutostartInputs in;
    in.packageMode = ProxorGui::CurrentPackageMode();
    in.fromLauncher = qEnvironmentVariable("NKR_FROM_LAUNCHER") == "1";
    in.launcherPath = QApplication::applicationDirPath() + "/launcher";
    in.appImagePath = QProcessEnvironment::systemEnvironment().value("APPIMAGE");
    in.applicationFilePath = QApplication::applicationFilePath();
    // Only when the wrapper really exists: otherwise the GUI binary is the best we have.
    const QString wrapper = ProxorPlatform::NativeWrapperPathFor(ProxorGui::PackageRootPath());
    if (QFileInfo::exists(wrapper)) in.nativeWrapperPath = wrapper;
    in.useAppdata = ProxorGui::dataStore->flag_use_appdata;
    in.appdataDir = ProxorGui::dataStore->appdataDir;
    return in;
}

static void logAutostartFailure(const QString &path) {
    if (MW_show_log) MW_show_log(QObject::tr("Start with system: cannot write %1").arg(path));
}

// Flatpak: the sandbox's own autostart directory is never seen by the host session, so the desktop is asked
// through the Background portal. This marker records what the desktop granted (it is the checkbox state).
static QString flatpakAutostartMarker() {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + QLatin1String("/flatpak-autostart");
}

static void SetFlatpakAutostart(bool enable) {
    // Older versions wrote a file the host never reads; clean it up once.
    QFile::remove(autostartFilePath());
    if (!ProxorPlatform::ShouldRequestFlatpakAutostart(enable, AutoRun_IsEnabled())) return;

    const auto commandline = ProxorPlatform::FlatpakAutostartCommandline(ProxorGui::dataStore->flag_use_appdata,
                                                                        ProxorGui::dataStore->appdataDir);
    ProxorDesktop::RequestAutostart(
        enable, commandline, QObject::tr("Start Proxor in the tray when you log in."), qApp,
        [enable](const ProxorDesktop::AutostartResult &r) {
            const QString marker = flatpakAutostartMarker();
            if (r.result.outcome == ProxorDesktop::PortalOutcome::Granted) {
                if (r.autostart) {
                    QDir().mkpath(QFileInfo(marker).absolutePath());
                    QFile f(marker);
                    if (f.open(QIODevice::WriteOnly)) f.close();
                } else {
                    QFile::remove(marker);
                }
                if (MW_show_log) {
                    MW_show_log(r.autostart ? QObject::tr("Start with system: the desktop will start Proxor when you log in.")
                                            : QObject::tr("Start with system: turned off."));
                }
                return;
            }
            if (enable) {
                QFile::remove(marker);
                MessageBoxWarning(software_name,
                                  QObject::tr("Proxor could not be set to start when you log in: %1 You can allow it in your "
                                              "desktop settings (GNOME: Settings > Apps > Proxor > Run in Background; "
                                              "KDE: System Settings > Autostart) and turn the option on again.")
                                      .arg(r.result.detail));
            } else {
                MessageBoxWarning(software_name,
                                  QObject::tr("Proxor could not stop starting when you log in: %1 Remove it in your "
                                              "desktop's autostart settings.")
                                      .arg(r.result.detail));
            }
        });
}

void AutoRun_SetEnabled(bool enable) {
    if (IsFlatpak(ProxorGui::CurrentPackageMode())) {
        SetFlatpakAutostart(enable);
        return;
    }
    // From https://github.com/nextcloud/desktop/blob/master/src/common/utility_unix.cpp
    QString appName = QCoreApplication::applicationName();
    QString userAutoStartPath = getUserAutostartDir_private();
    QString desktopFileLocation = autostartFilePath();

    if (enable) {
        if (!QDir().exists(userAutoStartPath) && !QDir().mkpath(userAutoStartPath)) {
            logAutostartFailure(userAutoStartPath);
            return;
        }

        QFile iniFile(desktopFileLocation);
        if (!iniFile.open(QIODevice::WriteOnly)) {
            logAutostartFailure(desktopFileLocation);
            return;
        }

        const auto command = ProxorPlatform::LinuxAutostartCommand(CurrentAutostartInputs());
        iniFile.write(ProxorPlatform::LinuxAutostartDesktopEntry(appName, command).toUtf8());
        iniFile.close();
    } else {
        QFile::remove(desktopFileLocation);
    }
}

bool AutoRun_IsEnabled() {
    if (IsFlatpak(ProxorGui::CurrentPackageMode())) return QFile::exists(flatpakAutostartMarker());
    return QFile::exists(autostartFilePath());
}

QString AutoRun_RefreshStaleEntry() {
    if (IsFlatpak(ProxorGui::CurrentPackageMode())) return {}; // the Background portal owns it

    QFile file(autostartFilePath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return {};
    const QString text = QString::fromUtf8(file.readAll());
    file.close();

    // The user (or the desktop's startup-apps tool) turned it off: leave it alone.
    if (ProxorPlatform::IsAutostartEntryDisabled(text)) return {};

    const auto existing = ProxorPlatform::ParseDesktopEntryExec(text);
    const auto expected = ProxorPlatform::LinuxAutostartCommand(CurrentAutostartInputs());
    if (!ProxorPlatform::ShouldRefreshAutostart(existing, expected, QApplication::applicationFilePath())) return {};

    // Keep unknown keys: change only the Exec line. No Exec line at all -> regenerate.
    QString updated = existing.isEmpty()
        ? ProxorPlatform::LinuxAutostartDesktopEntry(QCoreApplication::applicationName(), expected)
        : ProxorPlatform::ReplaceDesktopEntryExec(text, expected);
    QFile out(autostartFilePath());
    if (!out.open(QIODevice::WriteOnly)) {
        logAutostartFailure(autostartFilePath());
        return {};
    }
    out.write(updated.toUtf8());
    out.close();
    return "Start with system: updated the autostart entry to run " + expected.first() +
           " (the old entry could not start Proxor).";
}

#endif
