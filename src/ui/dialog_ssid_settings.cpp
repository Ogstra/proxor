#include "dialog_ssid_settings.h"
#include "ui_dialog_ssid_settings.h"

#include "main/GuiUtils.hpp"
#include "main/ProxorGui.hpp"

#include <QLabel>
#include <QCheckBox>

DialogSSIDSettings::DialogSSIDSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogSSIDSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
    D_LOAD_BOOL(ssid_on_demand_enabled)
    for (const auto &ssid : ProxorGui::dataStore->ssid_trigger_list) {
        ui->ssid_list_widget->addItem(ssid);
    }
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DialogSSIDSettings::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

DialogSSIDSettings::~DialogSSIDSettings() {
    delete ui;
}

void DialogSSIDSettings::on_btn_add_ssid_clicked() {
    auto ssid = ui->ssid_input->text().trimmed();
    if (ssid.isEmpty()) return;
    for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
        if (ui->ssid_list_widget->item(i)->text() == ssid) return;
    }
    ui->ssid_list_widget->addItem(ssid);
    ui->ssid_input->clear();
}

void DialogSSIDSettings::on_btn_remove_ssid_clicked() {
    auto items = ui->ssid_list_widget->selectedItems();
    for (auto *item : items) { delete item; }
}

void DialogSSIDSettings::accept() {
    D_SAVE_BOOL(ssid_on_demand_enabled)
    QStringList list;
    for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
        list << ui->ssid_list_widget->item(i)->text();
    }
    ProxorGui::dataStore->ssid_trigger_list = list;
    ProxorGui::dataStore->Save();
    QDialog::accept();
}
