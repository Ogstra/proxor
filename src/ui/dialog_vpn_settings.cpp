#include "dialog_vpn_settings.h"
#include "ui_dialog_vpn_settings.h"

#include "main/GuiUtils.hpp"
#include "main/ProxorGui.hpp"
#include "platform/PlatformCapabilitiesApp.hpp"
#include "platform/CapabilityUi.hpp"
#include "platform/PlatformCapabilities.hpp"
#include "platform/ProcessNames.hpp"
#include "ui/mainwindow_interface.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QSet>
#include <QVBoxLayout>

#include <algorithm>

#ifdef Q_OS_MACOS
#include "sys/macos/MacHelperClient.h"
#include "sys/macos/MacHelperService.h"
#include "sys/macos/MacHelperInstaller.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>

#include <memory>
#endif

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
    {
        auto *bypassClients = new QCheckBox(tr("Keep WireGuard, OpenVPN and Tailscale outside the tunnel"), ui->gb_process_name);
        bypassClients->setObjectName(QStringLiteral("vpn_bypass_vpn_clients"));
        bypassClients->setToolTip(tr("On: these VPN clients connect directly. Off: their traffic goes through the proxy, for example to carry WireGuard over a VLESS Reality proxy."));
        bypassClients->setChecked(ProxorGui::dataStore->vpn_bypass_vpn_clients);
        ui->verticalLayout_4->insertWidget(0, bypassClients);
        // Widgets added in code are not part of the .ui tab stops: put them in reading order.
        setTabOrder(ui->vpn_rule_cidr, ui->btn_pick_process);
        setTabOrder(ui->btn_pick_process, bypassClients);
        setTabOrder(bypassClients, ui->vpn_rule_process);
    }
    ui->single_core->setChecked(ProxorGui::dataStore->vpn_internal_tun);
#ifdef Q_OS_MACOS
    // The single-core Tun option does not apply on macOS (Tun runs in the Proxor service).
    ui->single_core->setVisible(false);
    {
        auto *box = new QGroupBox(tr("Proxor service"), this);
        auto *boxLayout = new QVBoxLayout(box);
        auto *status = new QLabel(box);
        status->setWordWrap(true);
        auto *buttons = new QHBoxLayout();
        auto *install = new QPushButton(box);
        auto *remove = new QPushButton(tr("Remove Proxor service"), box);
        buttons->addWidget(install);
        buttons->addWidget(remove);
        buttons->addStretch();
        boxLayout->addWidget(status);
        boxLayout->addLayout(buttons);

        // Insert above the bottom row (whitelist / troubleshooting / OK-Cancel).
        if (auto *vbox = qobject_cast<QVBoxLayout *>(layout()))
            vbox->insertWidget(std::max(0, vbox->count() - 1), box);
        else
            layout()->addWidget(box);

        auto state = std::make_shared<MacHelperState>(MacHelperState::NotInstalled);

        QPointer<DialogVPNSettings> self(this);

        // Copyable and self-contained: also captured by the async install/remove callbacks. The probe never
        // blocks the UI thread; the classification is applied when it answers (and only if the dialog is alive).
        const auto refresh = [self, status, install, remove, state]() {
            if (!self) return;
            status->setText(tr("Checking..."));
            status->setToolTip(QString());
            install->setEnabled(false);
            remove->setEnabled(false);
            MacHelperSvc()->probe(self.data(), 1000, [self, status, install, remove, state](const MacHelperProbe &probe, MacHelperState st) {
                if (!self) return;
                *state = st;
                install->setVisible(true);
                install->setEnabled(true);
                remove->setEnabled(true);
                switch (st) {
                case MacHelperState::NotInstalled:
                    status->setText(tr("Not installed"));
                    status->setToolTip(tr("Turning on Tun Mode or System Proxy installs it (one administrator password prompt)."));
                    install->setText(tr("Install"));
                    remove->setEnabled(false);
                    break;
                case MacHelperState::InstalledNotRunning:
                    status->setText(tr("Installed but not running. Allow Proxor in System Settings > General > Login Items & Extensions, or reinstall."));
                    install->setText(tr("Reinstall"));
                    break;
                case MacHelperState::NotAuthorized:
                    status->setText(tr("Installed by another user of this Mac; it does not accept this user yet."));
                    install->setText(tr("Allow this user"));
                    // Removing it would break the other user's Tun; they can remove it themselves.
                    remove->setEnabled(false);
                    break;
                case MacHelperState::Outdated:
                    status->setText(tr("Installed, needs an update for this version of Proxor."));
                    install->setText(tr("Update"));
                    break;
                case MacHelperState::Ready:
                    status->setText(tr("Running (service %1, sing-box %2).").arg(probe.build, probe.singbox));
                    install->setVisible(false);
                    break;
                }
            });
        };

        connect(install, &QPushButton::clicked, this, [this, self, refresh, state] {
            if (MacHelperInstaller::InstallInProgress()) {
                MessageBoxInfo(software_name, tr("The Proxor service installation is already waiting for your answer."));
                return;
            }
            MacHelperInstaller::ConfirmAndInstall(
                this, tr("Tun Mode"), DecideMacHelperEnable(*state), [self, refresh](MacAdminScriptResult r) {
                    if (!self) return;
                    refresh();
                    if (r.outcome == MacAdminScriptOutcome::Failed)
                        MessageBoxWarning(software_name, tr("The Proxor service could not be installed: %1").arg(r.reason));
                });
        });

        connect(remove, &QPushButton::clicked, this, [this, self, refresh] {
            const auto answer = QMessageBox::question(
                this, tr("Remove Proxor service"),
                tr("Remove the Proxor service? Tun Mode and System Proxy will be turned off. You can install it again later."));
            if (answer != QMessageBox::Yes) return;
            GetMainWindow()->proxor_set_spmode_vpn(false);
            GetMainWindow()->proxor_set_spmode_system_proxy(false);
            MacHelperInstaller::Uninstall(this, [self, refresh](MacAdminScriptResult r) {
                if (!self) return;
                refresh();
                if (r.outcome == MacAdminScriptOutcome::Failed)
                    MessageBoxWarning(software_name, tr("The Proxor service could not be removed: %1").arg(r.reason));
            });
        });

        refresh();
    }
#endif
    {
        using namespace ProxorPlatform;
        auto *tunNote = MakeCapabilityNote(this);
        ui->verticalLayout->insertWidget(0, tunNote);
        const auto tun = CurrentCapability(Capability::TunMode);
        if (!IsUsable(tun)) {
            QList<QWidget *> pageWidgets;
            for (int i = 0; i < ui->verticalLayout->count(); i++) {
                auto *w = ui->verticalLayout->itemAt(i)->widget();
                if (w != nullptr && w != tunNote) pageWidgets << w;
            }
            ApplyCapability(pageWidgets, tun, tunNote);
        } else {
            QStringList reasons;
            for (const auto &entry : {std::make_pair(static_cast<QWidget *>(ui->strict_route), CurrentCapability(Capability::TunStrictRoute)),
                                      std::make_pair(static_cast<QWidget *>(ui->single_core), CurrentCapability(Capability::TunSingleCore))}) {
                ApplyCapability(entry.first, entry.second);
                if (entry.second.support != Support::Supported && !entry.second.reason.isEmpty()) reasons << entry.second.reason;
            }
            if (!reasons.isEmpty()) {
                tunNote->setText(reasons.join("\n"));
                tunNote->setVisible(true);
            }
        }
    }
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
        QSet<QString> names;
        int skippedNames = 0;
        const bool readProc = ProxorPlatform::CompiledHostOs() == ProxorPlatform::HostOs::Linux;
        if (readProc) {
            const auto list = ProxorPlatform::ListLinuxProcessNames();
            for (const auto &n : list.names) names.insert(n);
            skippedNames = list.skipped;
        }
        QProcess proc;
        if (!readProc) {
#ifdef Q_OS_WIN
        proc.start("tasklist", {"/fo", "csv", "/nh"});
#else
        proc.start("ps", {"-eo", "comm"});
#endif
        if (!proc.waitForFinished(4000)) return;
        }

        const QString out = readProc ? QString() : QString(proc.readAllStandardOutput());
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
#elif defined(Q_OS_MACOS)
            // macOS `ps -o comm` prints the full executable path (plus a "COMM" header), while
            // sing-box process_name rules match the executable's file name.
            if (trimmed == QLatin1String("COMM")) continue;
            const auto name = trimmed.section(QLatin1Char('/'), -1);
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
        if (skippedNames > 0) {
            auto *note = new QLabel(tr("%n process(es) of other users are not listed: their full names cannot be read without administrator rights.", "", skippedNames), dlg);
            note->setWordWrap(true);
            layout->addWidget(note);
        }
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
#ifdef Q_OS_MACOS
    // On macOS the manual move overlapped the group box border; MacDialogs.cpp lays the button out
    // under the text box instead.
#else
    constexpr int margin = 8;
    auto *button = ui->btn_pick_process;
    const auto size = button->sizeHint();
    button->resize(size);
    button->move(std::max(margin, ui->gb_process_name->width() - size.width() - margin), 0);
    button->raise();
#endif
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
    if (auto *bypassClients = findChild<QCheckBox *>(QStringLiteral("vpn_bypass_vpn_clients"))) {
        ProxorGui::dataStore->vpn_bypass_vpn_clients = bypassClients->isChecked();
    }
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
                                      tr("If you have trouble starting Tun, you can force reset proxor_core process here.\n\n"
                                         "If it still does not work, restart the application with administrator privileges and try again."),
                                      tr("Reset"), tr("Cancel"), "",
                                      1, 1);
    if (r == 0) {
        GetMainWindow()->StopVPNProcess(true);
    }
}
