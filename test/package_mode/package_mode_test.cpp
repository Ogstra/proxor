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

QTEST_MAIN(PackageModeTest)
#include "package_mode_test.moc"
