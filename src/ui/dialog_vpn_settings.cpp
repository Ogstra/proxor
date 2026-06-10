#include "dialog_vpn_settings.h"
#include "ui_dialog_vpn_settings.h"

#include "main/GuiUtils.hpp"
#include "main/ProxorGui.hpp"
#include "ui/mainwindow_interface.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>

DialogVPNSettings::DialogVPNSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogVPNSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
    ui->gb_process_name->installEventFilter(this);
    positionPickProcessButton();

    ui->fake_dns->setChecked(ProxorGui::dataStore->fake_dns);
    ui->vpn_implementation->setCurrentIndex(ProxorGui::dataStore->vpn_implementation);
    ui->vpn_mtu->setCurrentText(Int2String(ProxorGui::dataStore->vpn_mtu));
    ui->vpn_ipv6->setChecked(ProxorGui::dataStore->vpn_ipv6);
    ui->hide_console->setChecked(ProxorGui::dataStore->vpn_hide_console);
#ifndef Q_OS_WIN
    ui->hide_console->setVisible(false);
#endif
    ui->strict_route->setChecked(ProxorGui::dataStore->vpn_strict_route);
    ui->single_core->setChecked(ProxorGui::dataStore->vpn_internal_tun);
    //
    D_LOAD_STRING_PLAIN(vpn_rule_cidr)
    D_LOAD_STRING_PLAIN(vpn_rule_process)
    //
    connect(ui->whitelist_mode, &QCheckBox::stateChanged, this, [=](int state) {
        if (state == Qt::Checked) {
            ui->gb_cidr->setTitle(tr("Proxy CIDR"));
            ui->gb_process_name->setTitle(tr("Proxy Process Name"));
        } else {
            ui->gb_cidr->setTitle(tr("Bypass CIDR"));
            ui->gb_process_name->setTitle(tr("Bypass Process Name"));
        }
    });
    ui->whitelist_mode->setChecked(ProxorGui::dataStore->vpn_rule_white);

    connect(ui->btn_pick_process, &QPushButton::clicked, this, [this] {
        QProcess proc;
#ifdef Q_OS_WIN
        proc.start("tasklist", {"/fo", "csv", "/nh"});
#else
        proc.start("ps", {"-eo", "comm"});
#endif
        if (!proc.waitForFinished(4000)) return;

        QSet<QString> names;
        const QString out = proc.readAllStandardOutput();
        for (const auto &line : out.split('\n')) {
            const auto trimmed = line.trimmed();
            if (trimmed.isEmpty()) continue;
#ifdef Q_OS_WIN
            const int comma = trimmed.indexOf(',');
            if (comma < 2) continue;
            auto name = trimmed.left(comma);
            if (name.startsWith('"') && name.endsWith('"'))
                name = name.mid(1, name.size() - 2);
            if (!name.isEmpty()) names.insert(name);
#else
            if (!trimmed.isEmpty()) names.insert(trimmed);
#endif
        }

        auto sorted = names.values();
        std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });

        auto *dlg = new QDialog(this);
        dlg->setWindowTitle(tr("Select Process"));
        dlg->resize(300, 400);
        auto *layout = new QVBoxLayout(dlg);
        auto *list = new QListWidget(dlg);
        list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        for (const auto &name : sorted)
            list->addItem(name);
        auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
        layout->addWidget(list);
        layout->addWidget(btns);
        connect(btns, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
        connect(list, &QListWidget::itemDoubleClicked, dlg, &QDialog::accept);

        if (dlg->exec() != QDialog::Accepted) return;

        QStringList current = ui->vpn_rule_process->toPlainText().split('\n', Qt::SkipEmptyParts);
        for (const auto *item : list->selectedItems()) {
            const auto name = item->text();
            if (!current.contains(name, Qt::CaseInsensitive))
                current.append(name);
        }
        ui->vpn_rule_process->setPlainText(current.join('\n'));
    });
}

DialogVPNSettings::~DialogVPNSettings() {
    delete ui;
}

bool DialogVPNSettings::eventFilter(QObject *watched, QEvent *event) {
    if (watched == ui->gb_process_name && event->type() == QEvent::Resize)
        positionPickProcessButton();
    return QDialog::eventFilter(watched, event);
}

void DialogVPNSettings::positionPickProcessButton() {
    constexpr int margin = 8;
    auto *button = ui->btn_pick_process;
    const auto size = button->sizeHint();
    button->resize(size);
    button->move(std::max(margin, ui->gb_process_name->width() - size.width() - margin), 0);
    button->raise();
}

void DialogVPNSettings::accept() {
    QStringList flags{"UpdateDataStore"};
    if (!save(flags)) return;
    MW_dialog_message("", flags.join(","));
    QDialog::accept();
}

bool DialogVPNSettings::save(QStringList &flags) {
    auto mtu = ui->vpn_mtu->currentText().toInt();
    if (mtu > 10000 || mtu < 1000) mtu = 9000;
    ProxorGui::dataStore->vpn_implementation = ui->vpn_implementation->currentIndex();
    ProxorGui::dataStore->fake_dns = ui->fake_dns->isChecked();
    ProxorGui::dataStore->vpn_mtu = mtu;
    ProxorGui::dataStore->vpn_ipv6 = ui->vpn_ipv6->isChecked();
    ProxorGui::dataStore->vpn_hide_console = ui->hide_console->isChecked();
    ProxorGui::dataStore->vpn_strict_route = ui->strict_route->isChecked();
    ProxorGui::dataStore->vpn_rule_white = ui->whitelist_mode->isChecked();
    bool isInternalChanged = ProxorGui::dataStore->vpn_internal_tun != ui->single_core->isChecked();
    ProxorGui::dataStore->vpn_internal_tun = ui->single_core->isChecked();
    //
    D_SAVE_STRING_PLAIN(vpn_rule_cidr)
    D_SAVE_STRING_PLAIN(vpn_rule_process)
    //
    if (isInternalChanged) {
        flags << "NeedRestart";
    } else {
        flags << "VPNChanged";
    }
    return true;
}

void DialogVPNSettings::on_troubleshooting_clicked() {
    auto r = QMessageBox::information(this, tr("Troubleshooting"),
                                      tr("If you have trouble starting VPN, you can force reset proxor_core process here.\n\n"
                                         "If it still does not work, restart the application with administrator privileges and try again."),
                                      tr("Reset"), tr("Cancel"), "",
                                      1, 1);
    if (r == 0) {
        GetMainWindow()->StopVPNProcess(true);
    }
}
