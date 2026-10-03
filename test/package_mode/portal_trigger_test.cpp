#include "platform/PortalShortcutTrigger.hpp"

#include <QtTest>

using ProxorPlatform::PortalTriggerFromKeySequence;

class PortalTriggerTest : public QObject {
    Q_OBJECT
private slots:
    void convert_data() {
        QTest::addColumn<QString>("in");
        QTest::addColumn<QString>("out");
        auto row = [](const char *in, const char *out) { QTest::newRow(*in ? in : "empty") << QString::fromUtf8(in) << QString::fromUtf8(out); };
        row("Ctrl+Alt+P", "CTRL+ALT+p");
        row("Shift+Ctrl+P", "CTRL+SHIFT+p");
        row("Meta+F5", "LOGO+F5");
        row("Ctrl+1", "CTRL+1");
        row("Ctrl+Space", "CTRL+space");
        row("Ctrl+Return", "CTRL+Return");
        row("Ctrl+Enter", "CTRL+Return");
        row("Ctrl+Esc", "CTRL+Escape");
        row("Ctrl+Backspace", "CTRL+BackSpace");
        row("Ctrl+Del", "CTRL+Delete");
        row("Ctrl+Ins", "CTRL+Insert");
        row("Ctrl+PgUp", "CTRL+Prior");
        row("Ctrl+PgDown", "CTRL+Next");
        row("Ctrl+Left", "CTRL+Left");
        row("Ctrl+Right", "CTRL+Right");
        row("Ctrl+Up", "CTRL+Up");
        row("Ctrl+Down", "CTRL+Down");
        row("Ctrl+Home", "CTRL+Home");
        row("Ctrl+End", "CTRL+End");
        row("Ctrl+Tab", "CTRL+Tab");
        row("Ctrl++", "CTRL+plus");
        row("Ctrl+,", "CTRL+comma");
        row("Ctrl+.", "CTRL+period");
        row("Ctrl+-", "CTRL+minus");
        row("", "");
        row("Ctrl+K, Ctrl+C", "");
        row("Ctrl+Alt", "");
        row("Ctrl+Foo", "");
    }
    void convert() {
        QFETCH(QString, in);
        QFETCH(QString, out);
        QCOMPARE(PortalTriggerFromKeySequence(in), out);
    }
};

QTEST_APPLESS_MAIN(PortalTriggerTest)
#include "portal_trigger_test.moc"
