#include "PackageMode.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class PackageModeTest final : public QObject {
    Q_OBJECT

private slots:
    void detectsWingetOnlyFromPackageRootMarker();
    void detectsFlatpakWithoutConfusingAppImage();
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

QTEST_MAIN(PackageModeTest)
#include "package_mode_test.moc"
