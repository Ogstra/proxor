#include "dialog_hotkey.h"
#include "ui_dialog_hotkey.h"

#include "ui/mainwindow_interface.h"

DialogHotkey::DialogHotkey(QWidget *parent) : QDialog(parent), ui(new Ui::DialogHotkey) {
    ui->setupUi(this);
    ui->show_mainwindow->setKeySequence(ProxorGui::dataStore->hotkey_mainwindow);
    ui->show_groups->setKeySequence(ProxorGui::dataStore->hotkey_group);
    ui->show_routes->setKeySequence(ProxorGui::dataStore->hotkey_route);
    ui->system_proxy->setKeySequence(ProxorGui::dataStore->hotkey_system_proxy_menu);
    GetMainWindow()->RegisterHotkey(true);
}

DialogHotkey::~DialogHotkey() {
    if (result() == QDialog::Accepted) {
        ProxorGui::dataStore->hotkey_mainwindow = ui->show_mainwindow->keySequence().toString();
        ProxorGui::dataStore->hotkey_group = ui->show_groups->keySequence().toString();
        ProxorGui::dataStore->hotkey_route = ui->show_routes->keySequence().toString();
        ProxorGui::dataStore->hotkey_system_proxy_menu = ui->system_proxy->keySequence().toString();
        ProxorGui::dataStore->Save();
    }
    GetMainWindow()->RegisterHotkey(false);
    delete ui;
}
