#pragma once

#include "PackageMode.hpp"

#include <QString>

struct PackageUpdateDecision {
    bool allowCheck;
    bool allowDownload;
    bool allowApply;
    bool allowUpdaterLaunch;
    QString guidance;
};

enum class FlatpakLifecycleEntryPoint {
    StartupRestore,
    MenuToggle,
    CleanupRestore,
};

struct FlatpakLifecycleDecision {
    bool allowTun;
    bool allowSystemProxy;
    bool requestPrivilegedOperation;
};

PackageUpdateDecision DecidePackageUpdate(PackageMode mode);
FlatpakLifecycleDecision DecideFlatpakLifecycle(PackageMode mode, FlatpakLifecycleEntryPoint entryPoint);

// Single composer for the guidance text shown in the update dialog: an empty template
// gives empty text, a template with %1 and a non-empty asset name is substituted, and a
// template with %1 and no asset name returns release-page wording instead, so no %1 ever
// reaches the UI.
QString UpdateGuidanceText(PackageMode mode, const QString &assetFileName);

struct UpdaterLaunchProbe {
    bool updaterExists;
    bool updaterIsExecutable;
    bool installDirWritable;
};

struct UpdaterLaunchDecision {
    bool canLaunch;
    QString reason;
};

UpdaterLaunchDecision DecideUpdaterLaunch(const UpdaterLaunchProbe &probe);

struct AppImageApplyProbe {
    bool appImagePathKnown;
    bool stagedFileExists;
    bool targetDirWritable;
    bool targetFileWritable;
};

struct AppImageApplyDecision {
    bool replaceTarget;
    QString reason;
};

// Pure replace-or-keep decision for the AppImage self-update. targetFileWritable is
// checked even though POSIX rename(2) only needs the containing directory to be
// writable: refusing to overwrite an AppImage the user cannot write to (a root-owned
// image sitting in a user-writable directory, for instance) is the conservative
// reading of the locked decision, and avoids silently replacing a file that is not
// the user's to replace.
AppImageApplyDecision DecideAppImageApply(const AppImageApplyProbe &probe);
