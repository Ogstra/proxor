#include "PackageMode.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

// Case- and whitespace-insensitive: the marker is written by shell tooling
// (stage-native-root.sh) that may add a trailing newline or run under a
// locale that differs from the one the app runs in.
QString ReadTrimmedLower(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed().toLower();
}

} // namespace

PackageMode DetectPackageMode(const QString &packageRoot,
                               const QString &flatpakId,
                               const QString &appImagePath,
                               const QString &nativeChannelMarkerPath) {
    // FLATPAK_ID is set by the Flatpak runtime itself, not by anything this process can
    // write, so it wins over every other signal.
    if (!flatpakId.trimmed().isEmpty()) {
        return PackageMode::Flatpak;
    }

    // APPIMAGE is set by the AppImage runtime and also names the file to replace on
    // self-update, so its mere presence is authoritative.
    if (!appImagePath.trimmed().isEmpty()) {
        return PackageMode::AppImage;
    }

    const auto wingetMarker = QDir(packageRoot).filePath(QStringLiteral("config/package-manager/winget"));
    if (QFileInfo(wingetMarker).isFile()) {
        return PackageMode::Winget;
    }

    // A native channel marker is written by the package itself. Detection never reads a
    // path prefix: /usr/local, an unpacked tarball in /opt and a Nix store path are
    // indistinguishable by prefix alone.
    if (!nativeChannelMarkerPath.isEmpty() && QFileInfo(nativeChannelMarkerPath).isFile()) {
        const auto content = ReadTrimmedLower(nativeChannelMarkerPath);
        if (content == QStringLiteral("deb")) return PackageMode::Deb;
        if (content == QStringLiteral("rpm")) return PackageMode::Rpm;
        if (content == QStringLiteral("arch")) return PackageMode::Arch;
        // A marker present at all means a distribution package owns these files.
        // Guessing "portable" here would hand a broken install a download button.
        return PackageMode::NativeUnknownManager;
    }

    return PackageMode::NativeOrPortable;
}

bool IsPackageManagerManaged(PackageMode mode) {
    switch (mode) {
        case PackageMode::Winget:
        case PackageMode::Flatpak:
        case PackageMode::Deb:
        case PackageMode::Rpm:
        case PackageMode::Arch:
        case PackageMode::NativeUnknownManager:
            return true;
        case PackageMode::AppImage:
        case PackageMode::NativeOrPortable:
            return false;
    }
    return false;
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

QString PackageModeName(PackageMode mode) {
    switch (mode) {
        case PackageMode::NativeOrPortable: return QStringLiteral("portable");
        case PackageMode::Winget: return QStringLiteral("winget");
        case PackageMode::Flatpak: return QStringLiteral("flatpak");
        case PackageMode::AppImage: return QStringLiteral("appimage");
        case PackageMode::Deb: return QStringLiteral("deb");
        case PackageMode::Rpm: return QStringLiteral("rpm");
        case PackageMode::Arch: return QStringLiteral("arch");
        case PackageMode::NativeUnknownManager: return QStringLiteral("unknown-package-manager");
    }
    return QStringLiteral("portable");
}
