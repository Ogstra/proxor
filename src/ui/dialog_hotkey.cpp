#include "dialog_hotkey.h"
#include "ui_dialog_hotkey.h"

#include "platform/CapabilityUi.hpp"
#include "platform/HotkeyReport.hpp"
#include "platform/PlatformCapabilitiesApp.hpp"
#include "ui/mainwindow_interface.h"

#include <QTimer>

DialogHotkey::DialogHotkey(QWidget *parent) : QDialog(parent), ui(new Ui::DialogHotkey) {
    ui->setupUi(this);
    hotkeysAtOpen = {ProxorGui::dataStore->hotkey_mainwindow, ProxorGui::dataStore->hotkey_group,
                     ProxorGui::dataStore->hotkey_route, ProxorGui::dataStore->hotkey_system_proxy_menu};
    ui->show_mainwindow->setKeySequence(ProxorGui::dataStore->hotkey_mainwindow);
    ui->show_groups->setKeySequence(ProxorGui::dataStore->hotkey_group);
    ui->show_routes->setKeySequence(ProxorGui::dataStore->hotkey_route);
    ui->system_proxy->setKeySequence(ProxorGui::dataStore->hotkey_system_proxy_menu);
    const auto status = ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::GlobalHotkeys);
    ProxorPlatform::ApplyCapability({ui->show_mainwindow, ui->show_groups, ui->show_routes, ui->system_proxy}, status,
                                    ui->hotkey_capability_note);
    GetMainWindow()->RegisterHotkey(true);
}

DialogHotkey::~DialogHotkey() {
    const auto problems = GetMainWindow()->RegisterHotkey(false);
    const auto status = ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::GlobalHotkeys);
    const QStringList hotkeysNow{ProxorGui::dataStore->hotkey_mainwindow, ProxorGui::dataStore->hotkey_group,
                                 ProxorGui::dataStore->hotkey_route, ProxorGui::dataStore->hotkey_system_proxy_menu};
    if (!problems.isEmpty() && status.support != ProxorPlatform::Support::Unsupported && hotkeysNow != hotkeysAtOpen) {
        if (status.support == ProxorPlatform::Support::Degraded) {
            // The desktop owns these hotkeys: say so in the log, not in a box on every close.
            if (MW_show_log) MW_show_log(QObject::tr("Hotkeys: %1").arg(problems.join("; ")));
        } else {
            // No modal box inside a destructor: show it once the event loop is back.
            QTimer::singleShot(0, GetMainWindow(), [problems] { MessageBoxWarning(QObject::tr("Hotkeys"), problems.join("\n")); });
        }
    }
    delete ui;
}

bool DialogHotkey::save(QStringList &flags) {
    Q_UNUSED(flags)
    const QList<ProxorPlatform::HotkeyBinding> bindings{
        {tr("Show main window"), ui->show_mainwindow->keySequence().toString()},
        {tr("Manage groups"), ui->show_groups->keySequence().toString()},
        {tr("Routing settings"), ui->show_routes->keySequence().toString()},
        {tr("System proxy menu"), ui->system_proxy->keySequence().toString()},
    };
    const auto plan = ProxorPlatform::PlanHotkeyRegistration(bindings, {ProxorPlatform::Support::Supported, {}});
    if (plan.hasDuplicates) {
        ui->hotkey_conflict_note->setText(plan.problems.join("\n"));
        ui->hotkey_conflict_note->show();
        return false;
    }
    ui->hotkey_conflict_note->hide();
    ProxorGui::dataStore->hotkey_mainwindow = bindings[0].sequence;
    ProxorGui::dataStore->hotkey_group = bindings[1].sequence;
    ProxorGui::dataStore->hotkey_route = bindings[2].sequence;
    ProxorGui::dataStore->hotkey_system_proxy_menu = bindings[3].sequence;
    return true;
}
