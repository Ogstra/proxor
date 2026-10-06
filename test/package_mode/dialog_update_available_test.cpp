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
    void homebrewGuidanceShowsBrewCommandWithCopyButton();
    void macAppGuidanceShowsSentenceOnly();
    void updateFailureTextNamesStageAndError();
    void keepsWorkingTextNamesTheVersion();
    void manualDownloadUrlPrefersTheReleasePage();
    void failedDialogShowsErrorKeepsWorkingAndUrl();
    void downloadManuallyOpensTheUrlAndCloses();
    void downloadManuallyStaysOpenWhenTheBrowserFails();
    void closeButtonRejects();
    void showUpdateFailedDialogIsNonModalAndDeletesOnClose();
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

void DialogUpdateAvailableTest::homebrewGuidanceShowsBrewCommandWithCopyButton() {
    const QString command = QStringLiteral("brew upgrade --cask proxor");
    DialogUpdateAvailable dialog("1.6.11", "proxor-1.6.12-macos-arm64.zip", "Release", {},
                                  /*allowUpdater=*/false, nullptr, command);

    auto *lineEdit = dialog.findChild<QLineEdit *>("lineEditGuidance");
    QVERIFY(lineEdit != nullptr);
    QCOMPARE(lineEdit->text(), QStringLiteral("brew upgrade --cask proxor"));
    QVERIFY(dialog.findChild<QPushButton *>("buttonCopyGuidance") != nullptr);
    QVERIFY(findButtonByText(dialog, QObject::tr("Download and Restart")) == nullptr);
}

void DialogUpdateAvailableTest::macAppGuidanceShowsSentenceOnly() {
    const QString sentence = QStringLiteral("Download proxor-1.6.12-macos-arm64.zip from the release page, quit Proxor, and replace Proxor.app in your Applications folder with the one inside the zip.");
    DialogUpdateAvailable dialog("1.6.11", "proxor-1.6.12-macos-arm64.zip", "Release", {},
                                  /*allowUpdater=*/false, nullptr, sentence);

    auto *label = dialog.findChild<QLabel *>("labelGuidanceIntro");
    QVERIFY(label != nullptr);
    QCOMPARE(label->text(), sentence);
    QVERIFY(label->text().contains(QStringLiteral("proxor-1.6.12-macos-arm64.zip")));
    QVERIFY(dialog.findChild<QLineEdit *>("lineEditGuidance") == nullptr);
    QVERIFY(dialog.findChild<QPushButton *>("buttonCopyGuidance") == nullptr);
    QVERIFY(findButtonByText(dialog, QObject::tr("Download and Restart")) == nullptr);
}

void DialogUpdateAvailableTest::updateFailureTextNamesStageAndError() {
    const auto check = UpdateFailureText(UpdateFailureStage::Check, "boom");
    QVERIFY(check.contains("Proxor could not check for updates: boom"));
    const auto dl = UpdateFailureText(UpdateFailureStage::Download, "boom");
    QVERIFY(dl.contains("The update could not be downloaded: boom"));
    const auto inst = UpdateFailureText(UpdateFailureStage::Install, "boom");
    QVERIFY(inst.contains("The update was downloaded but could not be installed: boom"));
    QVERIFY(UpdateFailureText(UpdateFailureStage::Download, "").contains("unknown error"));
    QVERIFY(UpdateFailureText(UpdateFailureStage::Check, "   ").contains("unknown error"));
}

void DialogUpdateAvailableTest::keepsWorkingTextNamesTheVersion() {
    const auto text = UpdateKeepsWorkingText("1.6.14");
    QVERIFY(text.contains("1.6.14"));
    QVERIFY(text.contains("keeps working"));
}

void DialogUpdateAvailableTest::manualDownloadUrlPrefersTheReleasePage() {
    const QString tag = "https://github.com/Ogstra/proxor/releases/tag/v1.6.14";
    QCOMPARE(UpdateManualDownloadUrl(tag), QUrl(tag));
    const QUrl fallback(QString::fromLatin1(kProxorReleasesPage));
    const QStringList bad = {"", "http://github.com/Ogstra/proxor/releases/tag/v1",
                             "https://evil.example/Ogstra/proxor/releases",
                             "https://github.com/Other/proxor/releases", "not a url"};
    for (const auto &b : bad) QCOMPARE(UpdateManualDownloadUrl(b), fallback);
}

void DialogUpdateAvailableTest::failedDialogShowsErrorKeepsWorkingAndUrl() {
    DialogUpdateFailed dialog(UpdateFailureStage::Download, "disk full", "1.6.14", "", nullptr,
                              [](const QUrl &) { return true; });
    QCOMPARE(dialog.windowTitle(), QStringLiteral("Update failed"));
    auto *err = dialog.findChild<QLabel *>("labelUpdateFailedError");
    QVERIFY(err != nullptr);
    QVERIFY(err->text().contains("The update could not be downloaded: disk full"));
    auto *keeps = dialog.findChild<QLabel *>("labelUpdateFailedKeepsWorking");
    QVERIFY(keeps != nullptr);
    QVERIFY(keeps->text().contains("1.6.14"));
    auto *edit = dialog.findChild<QLineEdit *>("lineEditManualUrl");
    QVERIFY(edit != nullptr);
    QVERIFY(edit->isReadOnly());
    QCOMPARE(edit->text(), dialog.manualDownloadUrl().toString());
    QVERIFY(findButtonByText(dialog, QStringLiteral("Download and Restart")) == nullptr);
}

void DialogUpdateAvailableTest::downloadManuallyOpensTheUrlAndCloses() {
    QUrl opened;
    DialogUpdateFailed dialog(UpdateFailureStage::Install, "x", "1.6.14", "", nullptr,
                              [&opened](const QUrl &u) { opened = u; return true; });
    auto *button = dialog.findChild<QPushButton *>("buttonDownloadManually");
    QVERIFY(button != nullptr);
    button->click();
    QCOMPARE(opened, QUrl(QString::fromLatin1(kProxorReleasesPage)));
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
}

void DialogUpdateAvailableTest::downloadManuallyStaysOpenWhenTheBrowserFails() {
    DialogUpdateFailed dialog(UpdateFailureStage::Install, "x", "1.6.14", "", nullptr,
                              [](const QUrl &) { return false; });
    auto *button = dialog.findChild<QPushButton *>("buttonDownloadManually");
    QVERIFY(button != nullptr);
    button->click();
    QVERIFY(dialog.result() != static_cast<int>(QDialog::Accepted));
}

void DialogUpdateAvailableTest::closeButtonRejects() {
    DialogUpdateFailed dialog(UpdateFailureStage::Check, "x", "1.6.14", "", nullptr,
                              [](const QUrl &) { return true; });
    auto *button = dialog.findChild<QPushButton *>("buttonUpdateFailedClose");
    QVERIFY(button != nullptr);
    button->click();
    QCOMPARE(dialog.result(), static_cast<int>(QDialog::Rejected));
}

void DialogUpdateAvailableTest::showUpdateFailedDialogIsNonModalAndDeletesOnClose() {
    ShowUpdateFailedDialog(nullptr, UpdateFailureStage::Download, "x", "1.6.14", "");
    QList<DialogUpdateFailed *> found;
    for (auto *w : QApplication::topLevelWidgets()) {
        if (auto *d = qobject_cast<DialogUpdateFailed *>(w); d && d->isVisible()) found << d;
    }
    QCOMPARE(found.size(), 1);
    QPointer<DialogUpdateFailed> dialog = found.first();
    QVERIFY(!dialog->isModal());
    QVERIFY(dialog->testAttribute(Qt::WA_DeleteOnClose));
    dialog->close();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QTest::qWait(0);
    QVERIFY(dialog.isNull());
}

QTEST_MAIN(DialogUpdateAvailableTest)
#include "dialog_update_available_test.moc"
