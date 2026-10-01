#pragma once

// Pure Linux autostart helpers (Qt Core only): which command the autostart entry runs,
// the entry text, and the decisions around repairing an entry written by an older version.

#include <QString>
#include <QStringList>

#include "main/PackageMode.hpp"

namespace ProxorPlatform {

struct AutostartInputs {
    PackageMode packageMode = PackageMode::NativeOrPortable;
    bool fromLauncher = false;
    QString launcherPath;        // <appDir>/launcher
    QString appImagePath;        // $APPIMAGE, empty when not an AppImage
    QString applicationFilePath; // the running GUI binary
    QString nativeWrapperPath;   // e.g. /usr/bin/proxor; empty when it is not on disk
    bool useAppdata = false;
    QString appdataDir;          // custom -appdata directory, may be empty
};

// Native channels run the wrapper (it sets QT_PLUGIN_PATH), an AppImage runs itself,
// the launcher runs through the launcher; otherwise the GUI binary. Then -tray and -appdata [dir].
QStringList LinuxAutostartCommand(const AutostartInputs &in);

// Only the Exec= line changes; every other line stays byte-identical.
QString ReplaceDesktopEntryExec(const QString &entryText, const QStringList &command);

// X-GNOME-Autostart-enabled=false or Hidden=true.
bool IsAutostartEntryDisabled(const QString &entryText);

// /usr/lib/proxor -> /usr/bin/proxor
QString NativeWrapperPathFor(const QString &packageRoot);

QString LinuxAutostartDesktopEntry(const QString &appName, const QStringList &command);

// Exec= value split and unquoted (Desktop Entry spec quoting); empty if none.
QStringList ParseDesktopEntryExec(const QString &entryText);

// True only when existing differs from expected and existing.first() == ownBinaryPath,
// i.e. this install wrote the entry that cannot start.
bool ShouldRefreshAutostart(const QStringList &existing, const QStringList &expected, const QString &ownBinaryPath);

} // namespace ProxorPlatform
