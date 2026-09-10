#include "PackageMode.hpp"
#include "PackagePolicy.hpp"

#include <QtTest>

class PackagePolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void wingetSuppressesEverySelfUpdateStep();
    void flatpakDisablesEveryPrivilegedLifecycleEntryPoint();
    void flatpakSearchesItsSharedDataFirst();
};

void PackagePolicyTest::wingetSuppressesEverySelfUpdateStep() {
    const auto decision = DecidePackageUpdate(PackageMode::Winget);
    QVERIFY(!decision.allowCheck);
    QVERIFY(!decision.allowDownload);
    QVERIFY(!decision.allowApply);
    QVERIFY(!decision.allowUpdaterLaunch);
    QCOMPARE(decision.guidance, QStringLiteral("winget upgrade Ogstra.Proxor"));
}

void PackagePolicyTest::flatpakDisablesEveryPrivilegedLifecycleEntryPoint() {
    for (const auto entryPoint : {FlatpakLifecycleEntryPoint::StartupRestore,
                                  FlatpakLifecycleEntryPoint::MenuToggle,
                                  FlatpakLifecycleEntryPoint::CleanupRestore}) {
        const auto decision = DecideFlatpakLifecycle(PackageMode::Flatpak, entryPoint);
        QVERIFY(!decision.allowTun);
        QVERIFY(!decision.allowSystemProxy);
        QVERIFY(!decision.requestPrivilegedOperation);
    }
}

void PackagePolicyTest::flatpakSearchesItsSharedDataFirst() {
    const auto paths = CoreAssetSearchPaths(PackageMode::Flatpak, "/app/lib/proxor");
    QVERIFY(!paths.isEmpty());
    QCOMPARE(paths.first(), QStringLiteral("/app/share/proxor"));
    QVERIFY(paths.contains(QStringLiteral("/usr/share/proxor")));
}

QTEST_MAIN(PackagePolicyTest)
#include "package_policy_test.moc"
