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
