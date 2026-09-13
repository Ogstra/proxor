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
