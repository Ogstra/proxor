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
