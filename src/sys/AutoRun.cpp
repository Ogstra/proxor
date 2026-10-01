#include "AutoRun.hpp"

#include <QApplication>
#include <QDir>

#include "3rdparty/fix_old_qt.h"
#include "main/ProxorGui.hpp"

// macOS headers (possibly OBJ-c)
#if defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
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

void AutoRun_SetEnabled(bool enable) {
    // From
    // https://github.com/nextcloud/desktop/blob/master/src/common/utility_mac.cpp
    QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);

    if (loginItems && enable) {
        // Insert an item to the list.
        LSSharedFileListItemRef item =
            LSSharedFileListInsertItemURL(loginItems, kLSSharedFileListItemLast, 0, 0, urlRef, 0, 0);

        if (item) CFRelease(item);

        CFRelease(loginItems);
    } else if (loginItems && !enable) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef);

        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef) CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr && itemUrlRef) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);

                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    LSSharedFileListItemRemove(loginItems, item); // remove it!
                }

                CFRelease(itemUrlRef);
            }
        }

        CFRelease(itemsArray);
        CFRelease(loginItems);
    }

    CFRelease(folderCFStr);
    CFRelease(urlRef);
}

bool AutoRun_IsEnabled() {
    // From
    // https://github.com/nextcloud/desktop/blob/master/src/common/utility_mac.cpp
    // this is quite some duplicate code with setLaunchOnStartup, at some
    // point we should fix this FIXME.
    bool returnValue = false;
    QString filePath = QDir(QCoreApplication::applicationDirPath() + QLatin1String("/../..")).absolutePath();
    CFStringRef folderCFStr = CFStringCreateWithCString(0, filePath.toUtf8().data(), kCFStringEncodingUTF8);
    CFURLRef urlRef = CFURLCreateWithFileSystemPath(0, folderCFStr, kCFURLPOSIXPathStyle, true);
    LSSharedFileListRef loginItems = LSSharedFileListCreate(0, kLSSharedFileListSessionLoginItems, 0);

    if (loginItems) {
        // We need to iterate over the items and check which one is "ours".
        UInt32 seedValue;
        CFArrayRef itemsArray = LSSharedFileListCopySnapshot(loginItems, &seedValue);
        CFStringRef appUrlRefString = CFURLGetString(urlRef); // no need for release

        for (int i = 0; i < CFArrayGetCount(itemsArray); i++) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef) CFArrayGetValueAtIndex(itemsArray, i);
            CFURLRef itemUrlRef = NULL;

            if (LSSharedFileListItemResolve(item, 0, &itemUrlRef, NULL) == noErr && itemUrlRef) {
                CFStringRef itemUrlString = CFURLGetString(itemUrlRef);

                if (CFStringCompare(itemUrlString, appUrlRefString, 0) == kCFCompareEqualTo) {
                    returnValue = true;
                }

                CFRelease(itemUrlRef);
            }
        }

        CFRelease(itemsArray);
    }

    CFRelease(loginItems);
    CFRelease(folderCFStr);
    CFRelease(urlRef);
    return returnValue;
}
QString AutoRun_RefreshStaleEntry() { return {}; }

#endif

#ifdef Q_OS_LINUX

#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include "platform/LinuxAutostart.hpp"

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

void AutoRun_SetEnabled(bool enable) {
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
