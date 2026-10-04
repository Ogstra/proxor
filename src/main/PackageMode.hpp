#pragma once

#include <QString>
#include <QStringList>

// Append only: the three original values keep their ordinals and meaning so the
// committed tests stay valid.
enum class PackageMode {
    NativeOrPortable,
    Winget,
    Flatpak,
    AppImage,
    Deb,
    Rpm,
    Arch,
    // A native channel marker exists but its content is not recognised. Distinct from
    // NativeOrPortable: a marker present at all means a distribution package owns these
    // files, and guessing "portable" there would hand a broken install a download button.
    NativeUnknownManager,
    Homebrew,   // macOS app installed by the Homebrew cask
    MacApp,     // macOS app installed any other way (release zip, local build)
};

// Grown with defaulted parameters so existing two-argument call sites keep compiling
// unchanged. Detection order: Flatpak env var, then appImagePath, then the winget
// marker under packageRoot, then the native channel marker, then portable.
PackageMode DetectPackageMode(const QString &packageRoot,
                               const QString &flatpakId,
                               const QString &appImagePath = {},
                               const QString &nativeChannelMarkerPath = {});
// macOS install channel. Pure (no Q_OS_ conditional): the only filesystem reads are of
// caskroomDirs. Homebrew when the bundle is exactly Proxor.app directly inside
// /Applications or <homeDir>/Applications AND some caskroom directory holds at least one
// non-hidden subdirectory (a version directory; ".metadata" is ignored); MacApp otherwise.
// appBundlePath need not exist on disk and is never resolved through symlinks.
PackageMode DetectMacPackageMode(const QString &appBundlePath,
                                 const QString &homeDir,
                                 const QStringList &caskroomDirs);
bool IsPackageManagerManaged(PackageMode mode);
bool IsFlatpak(PackageMode mode);
QStringList CoreAssetSearchPaths(PackageMode mode, const QString &packageRoot);

// Lowercase token used on the wire and in the log.
QString PackageModeName(PackageMode mode);
