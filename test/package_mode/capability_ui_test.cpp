#include "platform/CapabilityUi.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QtTest>

using namespace ProxorPlatform;

namespace {
CapabilityStatus S(Support s, const QString &reason = QString()) { return CapabilityStatus{s, reason}; }
} // namespace

class CapabilityUiTest : public QObject {
    Q_OBJECT
private slots:
    void unsupportedDisablesAndExplains() {
        QWidget parent;
        QCheckBox box(&parent);
        auto *note = MakeCapabilityNote(&parent);
        ApplyCapability(&box, S(Support::Unsupported, "nope"), note);
        QVERIFY(!box.isEnabled());
        QCOMPARE(box.toolTip(), QString("nope"));
        QVERIFY(!note->isHidden());
        QCOMPARE(note->text(), QString("nope"));
    }

    void degradedAndNeedsPermissionKeepEnabled() {
        for (auto support : {Support::Degraded, Support::NeedsPermission}) {
            QWidget parent;
            QCheckBox box(&parent);
            auto *note = MakeCapabilityNote(&parent);
            ApplyCapability(&box, S(support, "careful"), note);
            QVERIFY(box.isEnabled());
            QCOMPARE(box.toolTip(), QString("careful"));
            QVERIFY(!note->isHidden());
            QCOMPARE(note->text(), QString("careful"));
        }
    }

    void degradedDoesNotReEnableOrDisable() {
        QCheckBox box;
        box.setEnabled(false);
        ApplyCapability(&box, S(Support::Degraded, "careful"));
        QVERIFY(!box.isEnabled());
    }

    void supportedChangesNothing() {
        QWidget parent;
        QCheckBox box(&parent);
        box.setToolTip("mine");
        auto *note = MakeCapabilityNote(&parent);
        note->setText("stale");
        note->setVisible(true);
        ApplyCapability(&box, S(Support::Supported), note);
        QVERIFY(box.isEnabled());
        QCOMPARE(box.toolTip(), QString("mine"));
        QVERIFY(note->isHidden());
        QVERIFY(note->text().isEmpty());

        box.setEnabled(false);
        ApplyCapability(&box, S(Support::Supported));
        QVERIFY(!box.isEnabled());
    }

    void listOverloadAppliesToEveryControl() {
        QWidget parent;
        QCheckBox a(&parent), b(&parent);
        auto *note = MakeCapabilityNote(&parent);
        ApplyCapability(QList<QWidget *>{&a, nullptr, &b}, S(Support::Unsupported, "why"), note);
        QVERIFY(!a.isEnabled());
        QVERIFY(!b.isEnabled());
        QCOMPARE(a.toolTip(), QString("why"));
        QCOMPARE(b.toolTip(), QString("why"));
        QCOMPARE(note->text(), QString("why"));
        QVERIFY(!note->isHidden());
    }

    void nullControlAndNoteAreIgnored() {
        ApplyCapability(static_cast<QWidget *>(nullptr), S(Support::Unsupported, "x"), nullptr);
        ApplyCapability(QList<QWidget *>{nullptr}, S(Support::Unsupported, "x"), nullptr);
    }

    void makeCapabilityNoteIsHiddenWrappedAndNamed() {
        QWidget parent;
        auto *note = MakeCapabilityNote(&parent);
        QCOMPARE(note->objectName(), QString("capability_note"));
        QVERIFY(note->wordWrap());
        QVERIFY(note->isHidden());
        QCOMPARE(note->parent(), &parent);
    }
};

QTEST_MAIN(CapabilityUiTest)
#include "capability_ui_test.moc"
