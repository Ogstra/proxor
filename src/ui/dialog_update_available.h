#pragma once

#include <QDialog>
#include <QUrl>
#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui { class DialogUpdateAvailable; }
QT_END_NAMESPACE

class DialogUpdateAvailable : public QDialog {
    Q_OBJECT
public:
    enum Action { None, Download, OpenPage };

    explicit DialogUpdateAvailable(
        const QString &currentVersion,
        const QString &assetName,
        const QString &channel,
        const QString &releaseNote,
        bool allowUpdater,
        QWidget *parent = nullptr,
        const QString &managedGuidance = {});

    ~DialogUpdateAvailable() override;

    Action chosenAction() const { return m_action; }

private:
    Ui::DialogUpdateAvailable *ui;
    Action m_action = None;
};

// Where a failed in-app update stopped (phase 57).
enum class UpdateFailureStage { Check, Download, Install };

// Every Proxor release, prereleases included (/releases/latest would hide them); the fallback page.
inline constexpr const char *kProxorReleasesPage = "https://github.com/Ogstra/proxor/releases";

// First paragraph of the failure dialog; an empty error becomes "unknown error".
QString UpdateFailureText(UpdateFailureStage stage, const QString &error);
QString UpdateKeepsWorkingText(const QString &currentVersion);
// releaseUrl when it is a https://github.com/Ogstra/proxor/releases... URL, otherwise kProxorReleasesPage.
QUrl UpdateManualDownloadUrl(const QString &releaseUrl);

class DialogUpdateFailed : public QDialog {
    Q_OBJECT
public:
    using UrlOpener = std::function<bool(const QUrl &)>; // empty: QDesktopServices::openUrl
    DialogUpdateFailed(UpdateFailureStage stage, const QString &error, const QString &currentVersion,
                       const QString &releaseUrl, QWidget *parent = nullptr, UrlOpener openUrl = {});
    QUrl manualDownloadUrl() const { return m_url; }
private:
    QUrl m_url;
    UrlOpener m_openUrl;
};

// Single entry point for the Windows update error paths: non-modal, deletes itself on close.
void ShowUpdateFailedDialog(QWidget *parent, UpdateFailureStage stage, const QString &error,
                            const QString &currentVersion, const QString &releaseUrl);
