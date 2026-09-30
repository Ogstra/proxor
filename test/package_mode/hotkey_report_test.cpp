#include "platform/HotkeyReport.hpp"

#include <QtTest>

using namespace ProxorPlatform;

namespace {
const CapabilityStatus kOk{Support::Supported, {}};
QStringList Actions(const QList<HotkeyBinding> &l) {
    QStringList r;
    for (const auto &b : l) r << b.action;
    return r;
}
} // namespace

class HotkeyReportTest : public QObject {
    Q_OBJECT
private slots:
    void emptyBindingsAreSkipped() {
        auto p = PlanHotkeyRegistration({{"A", "Ctrl+Alt+P"}, {"B", ""}, {"C", "Ctrl+Alt+R"}}, kOk);
        QCOMPARE(Actions(p.toRegister), (QStringList{"A", "C"}));
        QVERIFY(p.problems.isEmpty());
        QVERIFY(!p.hasDuplicates);
    }
    void duplicatesAreSkippedAndReportedOnce() {
        auto p = PlanHotkeyRegistration({{"A", "Ctrl+Alt+P"}, {"B", "Ctrl+Alt+P"}, {"C", "Ctrl+Alt+R"}}, kOk);
        QCOMPARE(Actions(p.toRegister), (QStringList{"C"}));
        QVERIFY(p.hasDuplicates);
        QCOMPARE(p.problems.size(), 1);
        QVERIFY(p.problems[0].contains("Ctrl+Alt+P"));
        QVERIFY(p.problems[0].contains("A"));
        QVERIFY(p.problems[0].contains("B"));
    }
    void comparisonIgnoresCaseAndWhitespace() {
        auto p = PlanHotkeyRegistration({{"A", "Ctrl+Alt+P"}, {"B", "  ctrl+alt+p "}}, kOk);
        QVERIFY(p.hasDuplicates);
        QVERIFY(p.toRegister.isEmpty());
    }
    void unsupportedIsOneLine() {
        CapabilityStatus st{Support::Unsupported, "no Wayland"};
        auto p = PlanHotkeyRegistration({{"A", "Ctrl+P"}, {"B", "Ctrl+P"}}, st);
        QVERIFY(p.toRegister.isEmpty());
        QCOMPARE(p.problems, (QStringList{"Global hotkeys are off: no Wayland"}));
    }
    void unsupportedNothingConfigured() {
        CapabilityStatus st{Support::Unsupported, "no Wayland"};
        auto p = PlanHotkeyRegistration({{"A", ""}, {"B", " "}}, st);
        QVERIFY(p.toRegister.isEmpty());
        QVERIFY(p.problems.isEmpty());
    }
    void degradedRegistersAndNotes() {
        CapabilityStatus st{Support::Degraded, "XWayland only"};
        auto p = PlanHotkeyRegistration({{"A", "Ctrl+P"}}, st);
        QCOMPARE(Actions(p.toRegister), (QStringList{"A"}));
        QCOMPARE(p.problems, (QStringList{"XWayland only"}));
        auto q = PlanHotkeyRegistration({{"A", ""}}, st);
        QVERIFY(q.problems.isEmpty());
    }
    void rejectedText() {
        auto t = HotkeyRejectedText({"Show main window", "Ctrl+Alt+P"});
        QVERIFY(t.contains("Show main window"));
        QVERIFY(t.contains("Ctrl+Alt+P"));
        QVERIFY(t.contains("another"));
    }
};

QTEST_GUILESS_MAIN(HotkeyReportTest)
#include "hotkey_report_test.moc"
