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
        case PackageMode::Homebrew:
            // No trailing period: the dialog shows a string like this as a command with a Copy button.
            return {true, false, false, false, QStringLiteral("brew upgrade --cask proxor")};
        case PackageMode::MacApp:
            // No in-app self-update on macOS (ad-hoc signed, not notarized).
            return {true, false, false, false,
                    QStringLiteral("Download %1 from the release page, quit Proxor, and replace Proxor.app "
                                   "in your Applications folder with the one inside the zip.")};
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
        if (mode == PackageMode::MacApp) {
            return tmpl.arg(QStringLiteral("the macOS zip"));
        }
        return QStringLiteral("Update Proxor with the package manager that installed it; "
                               "the exact file name is on the release page.");
    }
    return tmpl;
}

bool UpdateIncludesPrereleases(PackageMode mode, bool userSetting) {
    // Every Proxor release for macOS is published as a prerelease.
    if (mode == PackageMode::Homebrew || mode == PackageMode::MacApp) {
        return true;
    }
    return userSetting;
}

QString PrereleaseSettingNote(PackageMode mode) {
    if (mode == PackageMode::Homebrew || mode == PackageMode::MacApp) {
        return QStringLiteral("Every Proxor release for macOS is published as a prerelease, "
                              "so on macOS the update check always includes prereleases.");
    }
    return {};
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

AppImageApplyDecision DecideAppImageApply(const AppImageApplyProbe &probe) {
    if (!probe.appImagePathKnown) {
        return {false, QStringLiteral("The path of the running AppImage is not known, so it cannot be replaced.")};
    }
    if (!probe.stagedFileExists) {
        return {false, QStringLiteral("The downloaded update is missing.")};
    }
    if (!probe.targetDirWritable) {
        return {false, QStringLiteral("The directory holding the AppImage is not writable.")};
    }
    if (!probe.targetFileWritable) {
        return {false, QStringLiteral("The AppImage file is not writable.")};
    }
    return {true, {}};
}
