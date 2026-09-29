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
};

// Grown with defaulted parameters so existing two-argument call sites keep compiling
// unchanged. Detection order: Flatpak env var, then appImagePath, then the winget
// marker under packageRoot, then the native channel marker, then portable.
PackageMode DetectPackageMode(const QString &packageRoot,
                               const QString &flatpakId,
                               const QString &appImagePath = {},
                               const QString &nativeChannelMarkerPath = {});
bool IsPackageManagerManaged(PackageMode mode);
bool IsFlatpak(PackageMode mode);
QStringList CoreAssetSearchPaths(PackageMode mode, const QString &packageRoot);

// Lowercase token used on the wire and in the log.
QString PackageModeName(PackageMode mode);
