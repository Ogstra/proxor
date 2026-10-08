#include "sys/macos/MacLoginItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

#include <unistd.h>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>

namespace ProxorMac {

QString LaunchAgentsDir() { return QDir::homePath() + QStringLiteral("/Library/LaunchAgents"); }

QString CurrentAppBundlePath() {
    return QDir(QCoreApplication::applicationDirPath() + QStringLiteral("/../..")).absolutePath();
}

ProxorPlatform::MacLoginItemStatus LegacyAgentStatus(const QString &plistPath) {
    @autoreleasepool {
        if (@available(macOS 13.0, *)) {
            NSURL *url = [NSURL fileURLWithPath:plistPath.toNSString()];
            const SMAppServiceStatus status = [SMAppService statusForLegacyURL:url];
            return ProxorPlatform::MapSMAppServiceStatus(static_cast<long>(status));
        }
        // macOS 12: no SMAppService/Background Task Management; launchd loads ~/Library/LaunchAgents at login.
        return QFileInfo::exists(plistPath) ? ProxorPlatform::MacLoginItemStatus::Enabled
                                            : ProxorPlatform::MacLoginItemStatus::NotRegistered;
    }
}

bool WriteLaunchAgent(const QString &plistPath, const QByteArray &xml, QString *error) {
    const QString dir = QFileInfo(plistPath).absolutePath();
    if (!QDir().mkpath(dir)) {
        if (error) *error = QStringLiteral("cannot create %1").arg(dir);
        return false;
    }
    QSaveFile file(plistPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(xml) != xml.size() || !file.commit()) {
        if (error) *error = file.errorString();
        return false;
    }
    QFile::setPermissions(plistPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup |
                                         QFileDevice::ReadOther);
    return true;
}

bool RemoveLaunchAgent(const QString &plistPath, const QString &label, QString *error) {
    QProcess launchctl;
    launchctl.start(QStringLiteral("/bin/launchctl"),
                    {QStringLiteral("bootout"), QStringLiteral("gui/%1/%2").arg(getuid()).arg(label)});
    if (!launchctl.waitForFinished(5000)) {
        launchctl.kill();
        launchctl.waitForFinished(1000);
    }
    QFile::remove(plistPath);
    if (QFile::exists(plistPath)) {
        if (error) *error = QStringLiteral("cannot remove %1").arg(plistPath);
        return false;
    }
    return true;
}

bool OpenLoginItemsSettings() {
    @autoreleasepool {
        if (@available(macOS 13.0, *)) {
            // No result value; it is the documented way, so count it as opened.
            [SMAppService openSystemSettingsLoginItems];
            return true;
        }
        // macOS 12: System Preferences > Users & Groups > Login Items (anchor UNVERIFIED on 12), then the pane itself.
        NSURL *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preferences.users?startupItems"];
        if (url && [[NSWorkspace sharedWorkspace] openURL:url]) return true;
        return [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:@"/System/Library/PreferencePanes/Accounts.prefPane"]];
    }
}

} // namespace ProxorMac
