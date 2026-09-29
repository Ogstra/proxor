#include "dialog_update_available.h"

#include <QApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtTest>

namespace {

QPushButton *findButtonByText(const QDialog &dialog, const QString &text) {
    for (auto *button : dialog.findChildren<QPushButton *>()) {
        if (button->text() == text) return button;
    }
    return nullptr;
}

} // namespace

class DialogUpdateAvailableTest final : public QObject {
    Q_OBJECT

private slots:
    void emptyGuidanceHasNoRowAndRespectsAllowUpdater();
    void commandGuidanceShowsCopyableReadOnlyFieldAndNoDownloadButton();
    void copyingGuidanceDoesNotCloseTheDialog();
    void sentenceGuidanceHasNoLineEdit();
    void guidanceRowSitsAboveButtonBox();
};

void DialogUpdateAvailableTest::emptyGuidanceHasNoRowAndRespectsAllowUpdater() {
    DialogUpdateAvailable dialog("1.6.6", "proxor-1.6.7-windows-x64.zip", "Release", {},
                                  /*allowUpdater=*/true, nullptr, /*managedGuidance=*/{});
    QVERIFY(dialog.findChild<QWidget *>("guidanceRow") == nullptr);
    QVERIFY(findButtonByText(dialog, QObject::tr("Download and Restart")) != nullptr);
}

void DialogUpdateAvailableTest::commandGuidanceShowsCopyableReadOnlyFieldAndNoDownloadButton() {
    const QString command = QStringLiteral("sudo apt install ./proxor_1.6.7-1_amd64.deb");
    DialogUpdateAvailable dialog("1.6.6", "proxor_1.6.7-1_amd64.deb", "Release", {},
                                  /*allowUpdater=*/false, nullptr, command);

    auto *guidanceRow = dialog.findChild<QWidget *>("guidanceRow");
    QVERIFY(guidanceRow != nullptr);

    auto *lineEdit = dialog.findChild<QLineEdit *>("lineEditGuidance");
    QVERIFY(lineEdit != nullptr);
    QVERIFY(lineEdit->isReadOnly());
    QCOMPARE(lineEdit->text(), command);

    QVERIFY(dialog.findChild<QPushButton *>("buttonCopyGuidance") != nullptr);
    QVERIFY(findButtonByText(dialog, QObject::tr("Download and Restart")) == nullptr);
    QVERIFY(findButtonByText(dialog, QObject::tr("Open Release Page")) != nullptr);
}

void DialogUpdateAvailableTest::copyingGuidanceDoesNotCloseTheDialog() {
    const QString command = QStringLiteral("sudo dnf install ./proxor-1.6.7-1.fc44.x86_64.rpm");
    DialogUpdateAvailable dialog("1.6.6", "proxor-1.6.7-1.fc44.x86_64.rpm", "Release", {},
                                  /*allowUpdater=*/false, nullptr, command);

    auto *copyButton = dialog.findChild<QPushButton *>("buttonCopyGuidance");
    QVERIFY(copyButton != nullptr);

    const bool visibleBefore = dialog.isVisible();
    copyButton->click();

    QCOMPARE(QGuiApplication::clipboard()->text(), command);
    QCOMPARE(dialog.isVisible(), visibleBefore);
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

void DialogUpdateAvailableTest::sentenceGuidanceHasNoLineEdit() {
    const QString sentence = QStringLiteral("Update Proxor with the package manager that installed it.");
    DialogUpdateAvailable dialog("1.6.6", "", "Release", {}, /*allowUpdater=*/false, nullptr, sentence);

    auto *guidanceRow = dialog.findChild<QWidget *>("guidanceRow");
    QVERIFY(guidanceRow != nullptr);

    auto *label = dialog.findChild<QLabel *>("labelGuidanceIntro");
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), sentence);

    QVERIFY(dialog.findChild<QLineEdit *>("lineEditGuidance") == nullptr);
    QVERIFY(dialog.findChild<QPushButton *>("buttonCopyGuidance") == nullptr);
}

void DialogUpdateAvailableTest::guidanceRowSitsAboveButtonBox() {
    DialogUpdateAvailable dialog("1.6.6", "proxor_1.6.7-1_amd64.deb", "Release", {},
                                  /*allowUpdater=*/false, nullptr,
                                  QStringLiteral("sudo apt install ./proxor_1.6.7-1_amd64.deb"));

    auto *guidanceRow = dialog.findChild<QWidget *>("guidanceRow");
    QVERIFY(guidanceRow != nullptr);
    auto *buttonBox = dialog.findChild<QWidget *>("buttonBox");
    QVERIFY(buttonBox != nullptr);

    auto *layout = qobject_cast<QVBoxLayout *>(dialog.layout());
    QVERIFY(layout != nullptr);

    int guidanceIndex = -1;
    int buttonBoxIndex = -1;
    for (int i = 0; i < layout->count(); ++i) {
        auto *widget = layout->itemAt(i)->widget();
        if (widget == guidanceRow) guidanceIndex = i;
        if (widget == buttonBox) buttonBoxIndex = i;
    }
    QVERIFY(guidanceIndex >= 0);
    QVERIFY(buttonBoxIndex >= 0);
    QVERIFY(guidanceIndex < buttonBoxIndex);
}

QTEST_MAIN(DialogUpdateAvailableTest)
#include "dialog_update_available_test.moc"
