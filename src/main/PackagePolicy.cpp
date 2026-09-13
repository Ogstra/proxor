#include "PackagePolicy.hpp"

PackageUpdateDecision DecidePackageUpdate(PackageMode mode) {
    // allowCheck is true for every channel: knowing a version exists is independent of
    // being able to apply it. Only download/apply/updater-launch differ per channel.
    switch (mode) {
        case PackageMode::Winget:
            // Never overwritten in place: winget's own recorded version would desync
            // from the one on disk.
            return {true, false, false, false, QStringLiteral("winget upgrade Ogstra.Proxor")};
        case PackageMode::Flatpak:
            // No --talk-name for a host spawn in the Flatpak manifest, so showing the
            // command is the only honest option.
            return {true, false, false, false, QStringLiteral("flatpak update io.github.Ogstra.Proxor")};
        case PackageMode::Deb:
            return {true, false, false, false, QStringLiteral("sudo apt install ./%1")};
        case PackageMode::Rpm:
            return {true, false, false, false, QStringLiteral("sudo dnf install ./%1")};
        case PackageMode::Arch:
            return {true, false, false, false, QStringLiteral("yay -S proxor")};
        case PackageMode::NativeUnknownManager:
            return {true, false, false, false,
                    QStringLiteral("Update Proxor with the package manager that installed it.")};
        case PackageMode::AppImage:
            // The AppImage updates itself by replacing the file $APPIMAGE points at;
            // it never launches the archive-only ./updater.
            return {true, true, true, false, {}};
        case PackageMode::NativeOrPortable:
            return {true, true, true, true, {}};
    }
    return {true, true, true, true, {}};
}

QString UpdateGuidanceText(PackageMode mode, const QString &assetFileName) {
    const auto decision = DecidePackageUpdate(mode);
    const auto &tmpl = decision.guidance;
    if (tmpl.isEmpty()) {
        return {};
    }
    if (tmpl.contains(QStringLiteral("%1"))) {
        if (!assetFileName.trimmed().isEmpty()) {
            return tmpl.arg(assetFileName);
        }
        // No asset name to substitute: never let a literal "%1" reach the UI.
        return QStringLiteral("Update Proxor with the package manager that installed it; "
                               "the exact file name is on the release page.");
    }
    return tmpl;
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
