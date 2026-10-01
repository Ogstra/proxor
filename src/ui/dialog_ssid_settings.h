#pragma once
#include <QDialog>
#include <QStringList>

#include "platform/WifiSsid.hpp"

class QLabel;
class QPushButton;

namespace Ui { class DialogSSIDSettings; }

class DialogSSIDSettings : public QDialog {
    Q_OBJECT
public:
    explicit DialogSSIDSettings(QWidget *parent = nullptr);
    ~DialogSSIDSettings() override;
public slots:
    void accept() override;
    bool save(QStringList &flags);
private slots:
    void on_btn_add_ssid_clicked();
    void on_btn_remove_ssid_clicked();
    void updateWifiStatus(const ProxorWifi::WifiReading &reading);
    void updatePermissionRow();
    void updateAddCurrentEnabled();
private:
    void populateProfileCombo();
    Ui::DialogSSIDSettings *ui;
    QLabel *m_wifiStatus = nullptr;
    QLabel *m_permissionNote = nullptr;
    QPushButton *m_addCurrent = nullptr;
    QPushButton *m_refreshWifi = nullptr;
    QPushButton *m_permissionButton = nullptr;
};
