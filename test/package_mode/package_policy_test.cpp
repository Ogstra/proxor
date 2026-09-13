#include "PackageMode.hpp"
#include "PackagePolicy.hpp"

#include <QtTest>

class PackagePolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void wingetAllowsCheckAndSuppressesEverySelfUpdateStep();
    void flatpakDisablesEveryPrivilegedLifecycleEntryPoint();
    void flatpakSearchesItsSharedDataFirst();
    void debStillReachesUsrShareProxor();
    void updaterLaunchIsRefusedForEveryUnmetCondition();
    void allowCheckIsTrueForEveryMode();
    void managedChannelsDisableDownloadApplyAndUpdaterLaunch();
    void appImageAllowsDownloadAndApplyButNotUpdaterLaunch();
    void updateGuidanceTextComposesDebCommandWithAssetName();
    void updateGuidanceTextFallsBackToReleasePageWordingWithoutAssetName();
    void updateGuidanceTextIsEmptyForPortableAndAppImage();
    void appImageApplyIsRefusedForEveryUnmetConditionInOrder();
};

void PackagePolicyTest::wingetAllowsCheckAndSuppressesEverySelfUpdateStep() {
    const auto decision = DecidePackageUpdate(PackageMode::Winget);
    QVERIFY(decision.allowCheck);
    QVERIFY(!decision.allowDownload);
    QVERIFY(!decision.allowApply);
    QVERIFY(!decision.allowUpdaterLaunch);
    QCOMPARE(decision.guidance, QStringLiteral("winget upgrade Ogstra.Proxor"));

    int updateDispatcherCalls = 0;
    const auto dispatch = [&updateDispatcherCalls](bool allowed) {
        if (allowed) ++updateDispatcherCalls;
    };
    dispatch(decision.allowDownload);
    dispatch(decision.allowApply);
    dispatch(decision.allowUpdaterLaunch);
    QCOMPARE(updateDispatcherCalls, 0);
}

void PackagePolicyTest::flatpakDisablesEveryPrivilegedLifecycleEntryPoint() {
    for (const auto entryPoint : {FlatpakLifecycleEntryPoint::StartupRestore,
                                  FlatpakLifecycleEntryPoint::MenuToggle,
                                  FlatpakLifecycleEntryPoint::CleanupRestore}) {
        const auto decision = DecideFlatpakLifecycle(PackageMode::Flatpak, entryPoint);
        QVERIFY(!decision.allowTun);
        QVERIFY(!decision.allowSystemProxy);
        QVERIFY(!decision.requestPrivilegedOperation);

        bool tunEnabled = true;
        bool systemProxyEnabled = true;
        int privilegedOperations = 0;
        if (!decision.allowTun) tunEnabled = false;
        if (!decision.allowSystemProxy) systemProxyEnabled = false;
        if (decision.requestPrivilegedOperation) ++privilegedOperations;
        QVERIFY(!tunEnabled);
        QVERIFY(!systemProxyEnabled);
        QCOMPARE(privilegedOperations, 0);
    }
}

void PackagePolicyTest::flatpakSearchesItsSharedDataFirst() {
    const auto paths = CoreAssetSearchPaths(PackageMode::Flatpak, "/app/lib/proxor");
    QVERIFY(!paths.isEmpty());
    QCOMPARE(paths.first(), QStringLiteral("/app/share/proxor"));
    QVERIFY(paths.contains(QStringLiteral("/usr/share/proxor")));
}

void PackagePolicyTest::debStillReachesUsrShareProxor() {
    const auto paths = CoreAssetSearchPaths(PackageMode::Deb, "/usr/lib/proxor");
    QVERIFY(paths.contains(QStringLiteral("/usr/share/proxor")));
}

void PackagePolicyTest::updaterLaunchIsRefusedForEveryUnmetCondition() {
    {
        const auto decision = DecideUpdaterLaunch({false, false, false});
        QVERIFY(!decision.canLaunch);
        QCOMPARE(decision.reason, QStringLiteral("The updater is not part of this installation."));
    }
    {
        const auto decision = DecideUpdaterLaunch({true, false, true});
        QVERIFY(!decision.canLaunch);
        QCOMPARE(decision.reason, QStringLiteral("The updater in this installation is not executable."));
    }
    {
        const auto decision = DecideUpdaterLaunch({true, true, false});
        QVERIFY(!decision.canLaunch);
        QCOMPARE(decision.reason, QStringLiteral("The installation directory is not writable."));
    }
    {
        const auto decision = DecideUpdaterLaunch({true, true, true});
        QVERIFY(decision.canLaunch);
        QVERIFY(decision.reason.isEmpty());
    }
}

void PackagePolicyTest::allowCheckIsTrueForEveryMode() {
    const PackageMode modes[] = {
        PackageMode::NativeOrPortable, PackageMode::Winget, PackageMode::Flatpak,
        PackageMode::AppImage,         PackageMode::Deb,    PackageMode::Rpm,
        PackageMode::Arch,             PackageMode::NativeUnknownManager,
    };
    for (const auto mode : modes) {
        QVERIFY(DecidePackageUpdate(mode).allowCheck);
    }
}

void PackagePolicyTest::managedChannelsDisableDownloadApplyAndUpdaterLaunch() {
    const PackageMode modes[] = {
        PackageMode::Winget, PackageMode::Flatpak,     PackageMode::Deb,
        PackageMode::Rpm,    PackageMode::Arch,        PackageMode::NativeUnknownManager,
    };
    for (const auto mode : modes) {
        const auto decision = DecidePackageUpdate(mode);
        QVERIFY(!decision.allowDownload);
        QVERIFY(!decision.allowApply);
        QVERIFY(!decision.allowUpdaterLaunch);
    }
}

void PackagePolicyTest::appImageAllowsDownloadAndApplyButNotUpdaterLaunch() {
    const auto decision = DecidePackageUpdate(PackageMode::AppImage);
    QVERIFY(decision.allowDownload);
    QVERIFY(decision.allowApply);
    QVERIFY(!decision.allowUpdaterLaunch);
}

void PackagePolicyTest::updateGuidanceTextComposesDebCommandWithAssetName() {
    QCOMPARE(UpdateGuidanceText(PackageMode::Deb, QStringLiteral("proxor_1.6.7-1_amd64.deb")),
             QStringLiteral("sudo apt install ./proxor_1.6.7-1_amd64.deb"));
}

void PackagePolicyTest::updateGuidanceTextFallsBackToReleasePageWordingWithoutAssetName() {
    const auto text = UpdateGuidanceText(PackageMode::Deb, {});
    QVERIFY(!text.isEmpty());
    QVERIFY(!text.contains(QStringLiteral("%1")));
}

void PackagePolicyTest::updateGuidanceTextIsEmptyForPortableAndAppImage() {
    QVERIFY(UpdateGuidanceText(PackageMode::NativeOrPortable, {}).isEmpty());
    QVERIFY(UpdateGuidanceText(PackageMode::AppImage, QStringLiteral("proxor-1.6.7-linux64.AppImage")).isEmpty());
}

void PackagePolicyTest::appImageApplyIsRefusedForEveryUnmetConditionInOrder() {
    {
        const auto decision = DecideAppImageApply({false, false, false, false});
        QVERIFY(!decision.replaceTarget);
        QCOMPARE(decision.reason,
                 QStringLiteral("The path of the running AppImage is not known, so it cannot be replaced."));
    }
    {
        const auto decision = DecideAppImageApply({true, false, false, false});
        QVERIFY(!decision.replaceTarget);
        QCOMPARE(decision.reason, QStringLiteral("The downloaded update is missing."));
    }
    {
        const auto decision = DecideAppImageApply({true, true, false, true});
        QVERIFY(!decision.replaceTarget);
        QCOMPARE(decision.reason, QStringLiteral("The directory holding the AppImage is not writable."));
    }
    {
        const auto decision = DecideAppImageApply({true, true, true, false});
        QVERIFY(!decision.replaceTarget);
        QCOMPARE(decision.reason, QStringLiteral("The AppImage file is not writable."));
    }
    {
        const auto decision = DecideAppImageApply({true, true, true, true});
        QVERIFY(decision.replaceTarget);
        QVERIFY(decision.reason.isEmpty());
    }
}

QTEST_MAIN(PackagePolicyTest)
#include "package_policy_test.moc"
