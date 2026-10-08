// macOS in-app update of a manually installed Proxor.app (phase 60).
// Qt Core only, pure; called from the macOS-only blocks of mainwindow*.cpp and tested on every runner.
#pragma once

#include <QString>
#include <QStringList>

namespace ProxorPlatform {

enum class MacAppUpdateRoute { Guidance, InPlace, Reveal };

struct MacAppUpdateProbe {
    bool macApp = false;            // CurrentPackageMode() == PackageMode::MacApp (Homebrew and all others: false)
    QString bundlePath;             // <applicationDirPath>/../.. cleaned
    bool parentWritable = false;    // the bundle's parent directory is writable by this user (filled by the caller)
    bool bundleOwnedByUser = false; // the bundle directory belongs to this user (filled by the caller)
    bool scriptPresent = false;     // the bundled relauncher exists
};

struct MacAppUpdateCapabilities { // compile-time facts from the phase 60 research decisions
    bool inPlace;                 // SWAP=rename2 and FALLBACK=none
    bool reveal;                  // FALLBACK=reveal
    bool requireOwner;            // WRITABLE=parent+owner
};

MacAppUpdateCapabilities MacAppUpdateBuildCapabilities();
MacAppUpdateRoute DecideMacAppUpdate(const MacAppUpdateProbe &probe, const MacAppUpdateCapabilities &caps);

bool IsTranslocatedBundlePath(const QString &bundlePath); // contains "/AppTranslocation/"
QString MacAppUpdateStageDir(const QString &bundlePath);  // "<parent>/.proxor-update"

inline constexpr const char *kMacAppUpdateScriptFromMacOSDir = "../Resources/update/proxor-app-update.sh";
inline constexpr const char *kMacAppUpdateResultFileName = "mac-update-result.txt";

// Drops argv[0], -tray, -flag_restart_tun_on, -flag_reorder and -psn_*.
QStringList MacAppUpdateRelaunchArgs(const QStringList &appArguments);

// {scriptPath, "install", "<pid>", zipPath, bundlePath, resultFile, "--", relaunchArgs...}
QStringList MacAppUpdateInstallArgs(const QString &scriptPath, qint64 pid, const QString &zipPath,
                                    const QString &bundlePath, const QString &resultFile,
                                    const QStringList &relaunchArgs);

// {scriptPath, "reveal", zipPath, destDir, resultFile} (used only when caps.reveal)
QStringList MacAppUpdateRevealArgs(const QString &scriptPath, const QString &zipPath, const QString &destDir,
                                   const QString &resultFile);

struct MacAppUpdateResult {
    bool present = false;
    bool ok = false;
    QString version;
    QString stage;
    QString message;
};

// "" -> not present; "ok 1.6.15" -> ok + version; "failed swap: msg" -> stage "swap", message "msg";
// "revealed <path>" -> ok, message = path; anything else -> present, not ok, message = text ("unknown error" if empty).
MacAppUpdateResult ParseMacAppUpdateResult(const QString &text);

} // namespace ProxorPlatform
