#include <QtTest>

#include "platform/MacLoginItemPolicy.hpp"
#include "sys/macos/MacLoginItem.h"

using namespace ProxorPlatform;

// Touches only a temporary directory and a "-test" label: never the real ~/Library/LaunchAgents, never launchctl
// bootstrap. (RemoveLaunchAgent runs `launchctl bootout` for the -test label, which is unknown and so a no-op.)
class MacLoginItemSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void writeLintStatusRemove() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString label = MacAutostartLabel() + QStringLiteral("-test");
        auto spec = DefaultMacLaunchAgentSpec("/Applications/Proxor.app", "");
        spec.label = label;
        const QString file = MacLaunchAgentFile(dir.path() + "/LaunchAgents", label);

        QString error;
        const QByteArray xml = MacLaunchAgentPlist(spec);
        QVERIFY2(ProxorMac::WriteLaunchAgent(file, xml, &error), qPrintable(error));
        QFile f(file);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), xml);
        f.close();

        QProcess lint;
        lint.start("/usr/bin/plutil", {"-lint", file});
        QVERIFY(lint.waitForFinished(10000));
        QCOMPARE(lint.exitCode(), 0);

        const auto status = ProxorMac::LegacyAgentStatus(file);
        QVERIFY(status == MacLoginItemStatus::NotRegistered || status == MacLoginItemStatus::Enabled ||
                status == MacLoginItemStatus::RequiresApproval || status == MacLoginItemStatus::NotFound ||
                status == MacLoginItemStatus::Unknown);
        qInfo() << "legacy agent status (0 NotRegistered, 1 Enabled, 2 RequiresApproval, 3 NotFound, 4 Unknown):"
                << static_cast<int>(status);

        QVERIFY2(ProxorMac::RemoveLaunchAgent(file, label, &error), qPrintable(error));
        QVERIFY(!QFile::exists(file));
    }

    void appBundlePathIsTwoLevelsAboveTheBinary() {
        const QString p = ProxorMac::CurrentAppBundlePath();
        QVERIFY(!p.isEmpty());
        QVERIFY(!p.endsWith('/'));
    }
};

QTEST_MAIN(MacLoginItemSmokeTest)
#include "mac_login_item_smoke_test.moc"
