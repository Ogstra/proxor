#pragma once

#include <QString>
#include <QStringList>

enum class PackageMode {
    NativeOrPortable,
    Winget,
    Flatpak,
};

PackageMode DetectPackageMode(const QString &packageRoot, const QString &flatpakId);
bool IsPackageManagerManaged(PackageMode mode);
bool IsFlatpak(PackageMode mode);
QStringList CoreAssetSearchPaths(PackageMode mode, const QString &packageRoot);
