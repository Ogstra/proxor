#pragma once

#include <QDialog>

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
        QWidget *parent = nullptr);

    ~DialogUpdateAvailable() override;

    Action chosenAction() const { return m_action; }

private:
    Ui::DialogUpdateAvailable *ui;
    Action m_action = None;
};
