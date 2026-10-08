#include "PackageMode.hpp"

#include <QDir>
#include <QFile>
#include <QList>
#include <QPair>
#include <QTemporaryDir>
#include <QtTest>

class PackageModeTest final : public QObject {
    Q_OBJECT

private slots:
    void detectsWingetOnlyFromPackageRootMarker();
    void detectsFlatpakWithoutConfusingAppImage();
    void detectsNativeChannelsFromMarkerContentCaseAndWhitespaceInsensitive();
    void unknownMarkerContentFailsClosedToUnknownManager();
    void missingMarkerAndNoEnvYieldsPortableRegardlessOfPathPrefix();
    void appImagePathYieldsAppImage();
    void flatpakWinsOverAppImagePath();
    void packageModeNameRoundTripsEveryMode();
    void macDetectsHomebrewInApplicationsAndUserApplications();
    void macFallsBackToMacAppWhenAnyConditionFails();
    void macModesAreNotPackageManagerManagedAndKeepAssetPaths();
};

void PackageModeTest::detectsWingetOnlyFromPackageRootMarker() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QDir().mkpath(root.filePath("config/package-manager")));

    QFile marker(root.filePath("config/package-manager/winget"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("winget\n"), qint64(7));

    QCOMPARE(DetectPackageMode(root.path(), {}), PackageMode::Winget);
    QVERIFY(IsPackageManagerManaged(PackageMode::Winget));

    QTemporaryDir portable;
    QVERIFY(portable.isValid());
    QFile unrelated(portable.filePath("config-package-manager-winget"));
    QVERIFY(unrelated.open(QIODevice::WriteOnly));
    QCOMPARE(DetectPackageMode(portable.path(), {}), PackageMode::NativeOrPortable);
}

void PackageModeTest::detectsFlatpakWithoutConfusingAppImage() {
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QCOMPARE(DetectPackageMode(root.path(), "io.github.Ogstra.Proxor"), PackageMode::Flatpak);
    QVERIFY(IsPackageManagerManaged(PackageMode::Flatpak));
    QVERIFY(IsFlatpak(PackageMode::Flatpak));
    QCOMPARE(DetectPackageMode(root.path(), {}), PackageMode::NativeOrPortable);
    QVERIFY(!IsFlatpak(PackageMode::NativeOrPortable));
}

void PackageModeTest::detectsNativeChannelsFromMarkerContentCaseAndWhitespaceInsensitive() {
    QTemporaryDir root;
    QVERIFY(root.isValid());

    const QList<QPair<QString, PackageMode>> cases = {
        {"deb\n", PackageMode::Deb},
        {"  RPM  ", PackageMode::Rpm},
        {"Arch", PackageMode::Arch},
    };
    for (const auto &testCase : cases) {
        QFile marker(root.filePath("package-channel"));
        QVERIFY(marker.open(QIODevice::WriteOnly | QIODevice::Truncate));
        marker.write(testCase.first.toUtf8());
        marker.close();

        QCOMPARE(DetectPackageMode(root.path(), {}, {}, marker.fileName()), testCase.second);
        QVERIFY(IsPackageManagerManaged(testCase.second));
    }
}

void PackageModeTest::unknownMarkerContentFailsClosedToUnknownManager() {
    QTemporaryDir root;
    QVERIFY(root.isValid());

    QFile marker(root.filePath("package-channel"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("snap\n");
    marker.close();

    QCOMPARE(DetectPackageMode(root.path(), {}, {}, marker.fileName()), PackageMode::NativeUnknownManager);
    QVERIFY(IsPackageManagerManaged(PackageMode::NativeUnknownManager));
}

void PackageModeTest::missingMarkerAndNoEnvYieldsPortableRegardlessOfPathPrefix() {
    // Detection never reads a path prefix: /usr/share/proxor looks like a system
    // install, but with no marker file it is still portable.
    QCOMPARE(DetectPackageMode(QStringLiteral("/usr/share/proxor"), {}, {}, {}), PackageMode::NativeOrPortable);
    QVERIFY(!IsPackageManagerManaged(PackageMode::NativeOrPortable));

    QTemporaryDir root;
    QVERIFY(root.isValid());
    QCOMPARE(DetectPackageMode(root.path(), {}, {}, root.filePath("no-such-marker")), PackageMode::NativeOrPortable);
}

void PackageModeTest::appImagePathYieldsAppImage() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QCOMPARE(DetectPackageMode(root.path(), {}, "/tmp/proxor.AppImage"), PackageMode::AppImage);
    QVERIFY(!IsPackageManagerManaged(PackageMode::AppImage));
}

void PackageModeTest::flatpakWinsOverAppImagePath() {
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QCOMPARE(DetectPackageMode(root.path(), "io.github.Ogstra.Proxor", "/tmp/proxor.AppImage"), PackageMode::Flatpak);
}

void PackageModeTest::packageModeNameRoundTripsEveryMode() {
    QCOMPARE(PackageModeName(PackageMode::NativeOrPortable), QStringLiteral("portable"));
    QCOMPARE(PackageModeName(PackageMode::Winget), QStringLiteral("winget"));
    QCOMPARE(PackageModeName(PackageMode::Flatpak), QStringLiteral("flatpak"));
    QCOMPARE(PackageModeName(PackageMode::AppImage), QStringLiteral("appimage"));
    QCOMPARE(PackageModeName(PackageMode::Deb), QStringLiteral("deb"));
    QCOMPARE(PackageModeName(PackageMode::Rpm), QStringLiteral("rpm"));
    QCOMPARE(PackageModeName(PackageMode::Arch), QStringLiteral("arch"));
    QCOMPARE(PackageModeName(PackageMode::NativeUnknownManager), QStringLiteral("unknown-package-manager"));
}

namespace {

// <tmp>/Caskroom/proxor/<entry>; entry is a directory name such as "1.6.11" or ".metadata".
QString MakeCaskroom(const QTemporaryDir &tmp, const QString &entry) {
    const QString dir = tmp.filePath(QStringLiteral("Caskroom/proxor"));
    if (!entry.isEmpty()) {
        QDir().mkpath(dir + QLatin1Char('/') + entry);
    }
    return dir;
}

} // namespace

void PackageModeTest::macDetectsHomebrewInApplicationsAndUserApplications() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString home = tmp.filePath(QStringLiteral("home"));
    const QString caskroom = MakeCaskroom(tmp, QStringLiteral("1.6.11"));
    QCOMPARE(DetectMacPackageMode(QStringLiteral("/Applications/Proxor.app"), home, {caskroom}), PackageMode::Homebrew);
    QCOMPARE(DetectMacPackageMode(home + QStringLiteral("/Applications/Proxor.app"), home, {caskroom}),
             PackageMode::Homebrew);
    // The first caskroom that qualifies is enough.
    QCOMPARE(DetectMacPackageMode(QStringLiteral("/Applications/Proxor.app"), home,
                                  {tmp.filePath(QStringLiteral("missing")), caskroom}),
             PackageMode::Homebrew);
}

void PackageModeTest::macFallsBackToMacAppWhenAnyConditionFails() {
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString home = tmp.filePath(QStringLiteral("home"));
    const QString app = QStringLiteral("/Applications/Proxor.app");
    // No caskroom directory at all.
    QCOMPARE(DetectMacPackageMode(app, home, {tmp.filePath(QStringLiteral("none"))}), PackageMode::MacApp);
    QCOMPARE(DetectMacPackageMode(app, home, {}), PackageMode::MacApp);
    // Caskroom holding only hidden entries.
    QCOMPARE(DetectMacPackageMode(app, home, {MakeCaskroom(tmp, QStringLiteral(".metadata"))}), PackageMode::MacApp);

    QTemporaryDir other;
    QVERIFY(other.isValid());
    const QString caskroom = MakeCaskroom(other, QStringLiteral("1.6.11"));
    // A local build with a caskroom present.
    QCOMPARE(DetectMacPackageMode(QStringLiteral("/Users/x/dev/proxor/build-macos/Proxor.app"), home, {caskroom}),
             PackageMode::MacApp);
    // A renamed copy.
    QCOMPARE(DetectMacPackageMode(QStringLiteral("/Applications/Proxor 2.app"), home, {caskroom}), PackageMode::MacApp);
}

void PackageModeTest::macModesAreNotPackageManagerManagedAndKeepAssetPaths() {
    QCOMPARE(PackageModeName(PackageMode::Homebrew), QStringLiteral("homebrew"));
    QCOMPARE(PackageModeName(PackageMode::MacApp), QStringLiteral("macos-app"));
    QVERIFY(IsPackageManagerManaged(PackageMode::Homebrew));
    QVERIFY(!IsPackageManagerManaged(PackageMode::MacApp));
    QVERIFY(!IsFlatpak(PackageMode::Homebrew));
    QVERIFY(!IsFlatpak(PackageMode::MacApp));
    const QString root = QStringLiteral("/Applications/Proxor.app/Contents/MacOS");
    const auto expected = CoreAssetSearchPaths(PackageMode::NativeOrPortable, root);
    QCOMPARE(CoreAssetSearchPaths(PackageMode::Homebrew, root), expected);
    QCOMPARE(CoreAssetSearchPaths(PackageMode::MacApp, root), expected);
}

QTEST_MAIN(PackageModeTest)
#include "package_mode_test.moc"
