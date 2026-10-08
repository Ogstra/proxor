#include "platform/MacReopenPolicy.hpp"

#include <QtTest>

#include <vector>

using namespace ProxorPlatform;

namespace {
// Qt::ApplicationState as integers: 0 Suspended, 1 Hidden, 2 Inactive, 4 Active.
std::vector<bool> Replay(const std::vector<int> &states) {
    RepeatActiveDetector d;
    std::vector<bool> out;
    for (int s : states) out.push_back(d.onState(s));
    return out;
}
} // namespace

class MacReopenPolicyTest : public QObject {
    Q_OBJECT
private slots:
    void decide() {
        QCOMPARE(DecideReopen(false, false, false), ReopenAction::ShowWindow);
        QCOMPARE(DecideReopen(true, true, false), ReopenAction::ShowWindow);
        QCOMPARE(DecideReopen(false, true, false), ReopenAction::ShowWindow);
        QCOMPARE(DecideReopen(true, false, false), ReopenAction::None);
        QCOMPARE(DecideReopen(false, false, true), ReopenAction::None);
        QCOMPARE(DecideReopen(true, true, true), ReopenAction::None);
    }

    void detectorBasics() {
        QCOMPARE(Replay({4}), (std::vector<bool>{false}));
        QCOMPARE(Replay({2, 4}), (std::vector<bool>{false, false}));
        QCOMPARE(Replay({4, 4}), (std::vector<bool>{false, true}));
        QCOMPARE(Replay({4, 2, 4}), (std::vector<bool>{false, false, false}));
        QCOMPARE(Replay({4, 4, 4}), (std::vector<bool>{false, true, true}));
    }

    // Sequences recorded in 53-RESEARCH Q1 (qtprobe.log): launch Active, `open -a` while active gives
    // Active -> Active; later Inactive, then Inactive -> Active followed 38 ms later by Active -> Active.
    void recordedSequences() {
        QCOMPARE(Replay({4, 4, 2, 4, 4, 2}), (std::vector<bool>{false, true, false, false, true, false}));
        // Q4: idle launch, nothing after the first Active: the launch never fires.
        QCOMPARE(Replay({4, 2}), (std::vector<bool>{false, false}));
    }

    void logLine() {
        QCOMPARE(ReopenLogLine(), QStringLiteral("Dock: reopen, showing the main window"));
    }
};

QTEST_APPLESS_MAIN(MacReopenPolicyTest)
#include "mac_reopen_policy_test.moc"
