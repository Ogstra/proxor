#include "PackagePolicy.hpp"

PackageUpdateDecision DecidePackageUpdate(PackageMode mode) {
    if (mode == PackageMode::Winget) {
        return {false, false, false, false, QStringLiteral("winget upgrade Ogstra.Proxor")};
    }
    if (mode == PackageMode::Flatpak) {
        return {false, false, false, false, QStringLiteral("flatpak update")};
    }
    return {true, true, true, true, {}};
}

FlatpakLifecycleDecision DecideFlatpakLifecycle(PackageMode mode, FlatpakLifecycleEntryPoint) {
    if (IsFlatpak(mode)) {
        return {false, false, false};
    }
    return {true, true, true};
}

UpdaterLaunchDecision DecideUpdaterLaunch(const UpdaterLaunchProbe &probe) {
    if (!probe.updaterExists) {
        return {false, QStringLiteral("The updater is not part of this installation.")};
    }
    if (!probe.updaterIsExecutable) {
        return {false, QStringLiteral("The updater in this installation is not executable.")};
    }
    if (!probe.installDirWritable) {
        return {false, QStringLiteral("The installation directory is not writable.")};
    }
    return {true, {}};
}
