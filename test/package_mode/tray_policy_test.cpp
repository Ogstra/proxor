#include "platform/PlatformCapabilities.hpp"
#include "platform/TrayPolicy.hpp"

#include <QtTest>

using namespace ProxorPlatform;

namespace {
CapabilityStatus Yes() { return {}; }
CapabilityStatus No() { return CapabilityStatus{Support::Unsupported, QStringLiteral("No tray here.")}; }
} // namespace

class TrayPolicyTest : public QObject {
    Q_OBJECT
private slots:
    void closeAction() {
        QCOMPARE(DecideCloseAction(Yes()), CloseAction::HideToTray);
        QCOMPARE(DecideCloseAction(No()), CloseAction::Minimize);
    }

    void startupVisibility() {
        QCOMPARE(DecideStartupVisibility(false, Yes(), 0, 10000), StartupVisibility::ShowWindow);
        QCOMPARE(DecideStartupVisibility(false, No(), 0, 10000), StartupVisibility::ShowWindow);
        QCOMPARE(DecideStartupVisibility(true, Yes(), 0, 10000), StartupVisibility::StayHidden);
        QCOMPARE(DecideStartupVisibility(true, No(), 0, 10000), StartupVisibility::WaitForTray);
        QCOMPARE(DecideStartupVisibility(true, No(), 9500, 10000), StartupVisibility::WaitForTray);
        QCOMPARE(DecideStartupVisibility(true, No(), 10000, 10000), StartupVisibility::ShowWindow);
    }

    void noticesCarryTheReason() {
        const QString close = NoTrayCloseNotice(No());
        QVERIFY(close.contains("No tray here."));
        QVERIFY(close.contains("Exit"));
        const QString start = NoTrayStartupNotice(No());
        QVERIFY(start.contains("No tray here."));
        QVERIFY(start.contains("window visible"));
    }
};

QTEST_APPLESS_MAIN(TrayPolicyTest)
#include "tray_policy_test.moc"
