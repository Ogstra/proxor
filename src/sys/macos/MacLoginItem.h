#pragma once

// macOS "Start with system": a user LaunchAgent plus macOS's own login item status (phase 53).
// Implemented in MacLoginItem.mm (ARC); listed only in cmake/macos/macos.cmake.

#include <QByteArray>
#include <QString>

#include "platform/MacLoginItemPolicy.hpp"

namespace ProxorMac {

QString LaunchAgentsDir();      // ~/Library/LaunchAgents
QString CurrentAppBundlePath(); // QDir(applicationDirPath + "/../..").absolutePath()

// [SMAppService statusForLegacyURL:] for the agent plist, mapped through MapSMAppServiceStatus.
ProxorPlatform::MacLoginItemStatus LegacyAgentStatus(const QString &plistPath);

// mkpath + QSaveFile, mode 0644. Does not bootstrap the agent (RunAtLoad would reopen the running app).
bool WriteLaunchAgent(const QString &plistPath, const QByteArray &xml, QString *error);

// launchctl bootout gui/<uid>/<label> (result ignored: it is normally loaded but not running) + remove the file.
bool RemoveLaunchAgent(const QString &plistPath, const QString &label, QString *error);

// Opens System Settings > Login Items; true when something opened.
bool OpenLoginItemsSettings();

} // namespace ProxorMac
