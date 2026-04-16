#pragma once
#include <QDialog>

namespace Ui { class DialogSSIDSettings; }

class DialogSSIDSettings : public QDialog {
    Q_OBJECT
public:
    explicit DialogSSIDSettings(QWidget *parent = nullptr);
    ~DialogSSIDSettings() override;
public slots:
    void accept() override;
private slots:
    void on_btn_add_ssid_clicked();
    void on_btn_remove_ssid_clicked();
private:
    void populateProfileCombo();
    Ui::DialogSSIDSettings *ui;
};
