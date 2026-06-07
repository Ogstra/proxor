#ifndef DIALOG_BASIC_SETTINGS_H
#define DIALOG_BASIC_SETTINGS_H

#include <QDialog>
#include <QJsonObject>

namespace Ui {
    class DialogBasicSettings;
}

class QScrollArea;
class QListWidget;
class DialogManageRoutes;
class DialogVPNSettings;
class DialogSSIDSettings;
class DialogHotkey;

class DialogBasicSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogBasicSettings(QWidget *parent = nullptr);

    ~DialogBasicSettings();

public slots:

    void accept();

    void selectSection(const QString &title);

private:
    Ui::DialogBasicSettings *ui;

    struct {
        QJsonObject extraCore;
        QString custom_inbound;
        bool needRestart = false;
    } CACHE;

    DialogManageRoutes *m_routingPage = nullptr;
    DialogVPNSettings *m_vpnPage = nullptr;
    DialogSSIDSettings *m_ssidPage = nullptr;
    DialogHotkey *m_hotkeyPage = nullptr;
    QScrollArea *m_settingsScroll = nullptr;
    QListWidget *m_settingsNav = nullptr;

    void relayoutSettingsScroll();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:

    void refresh_auth();

    void on_set_custom_icon_clicked();

    void on_inbound_auth_clicked();

    void on_core_settings_clicked();
};

#endif // DIALOG_BASIC_SETTINGS_H
