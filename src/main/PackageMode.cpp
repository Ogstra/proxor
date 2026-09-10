#include "PackageMode.hpp"

#include <QDir>
#include <QFileInfo>

PackageMode DetectPackageMode(const QString &packageRoot, const QString &flatpakId) {
    if (!flatpakId.trimmed().isEmpty()) {
        return PackageMode::Flatpak;
    }

    const auto marker = QDir(packageRoot).filePath(QStringLiteral("config/package-manager/winget"));
    if (QFileInfo(marker).isFile()) {
        return PackageMode::Winget;
    }
    return PackageMode::NativeOrPortable;
}

bool IsPackageManagerManaged(PackageMode mode) {
    return mode == PackageMode::Winget || mode == PackageMode::Flatpak;
}

bool IsFlatpak(PackageMode mode) {
    return mode == PackageMode::Flatpak;
}

QStringList CoreAssetSearchPaths(PackageMode mode, const QString &packageRoot) {
    QStringList paths;
    if (IsFlatpak(mode)) {
        paths << QStringLiteral("/app/share/proxor");
    }
    if (!packageRoot.isEmpty()) {
        const QDir root(packageRoot);
        paths << root.filePath(QStringLiteral("config"));
        paths << root.absolutePath();
        paths << root.filePath(QStringLiteral("../share/proxor"));
    }
    paths << QStringLiteral("/usr/share/sing-geoip")
          << QStringLiteral("/usr/share/sing-geosite")
          << QStringLiteral("/usr/share/sing-box")
          << QStringLiteral("/usr/lib/proxor")
          << QStringLiteral("/usr/share/proxor");
    paths.removeDuplicates();
    return paths;
}
