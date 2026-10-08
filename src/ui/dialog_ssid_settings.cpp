#include "dialog_ssid_settings.h"
#include "ui_dialog_ssid_settings.h"

#include "db/Database.hpp"
#include "main/GuiUtils.hpp"
#include "main/ProxorGui.hpp"
#include "platform/PlatformCapabilitiesApp.hpp"
#include "platform/CapabilityUi.hpp"
#include "sys/WifiMonitor.hpp"
#include "sys/wifi/WifiPermission.hpp"

#include <memory>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

std::shared_ptr<ProxorGui::ProxyEntity> resolveSavedOnDemandProfile() {
    auto *dataStore = ProxorGui::dataStore;
    auto direct = ProxorGui::profileManager->GetProfile(dataStore->ssid_on_demand_profile_id);
    if (direct != nullptr) return direct;

    struct Candidate {
        std::shared_ptr<ProxorGui::ProxyEntity> profile;
        int score = 0;
        bool matchedName = false;
        bool matchedType = false;
        bool matchedAddress = false;
        bool matchedPort = false;
    };

    Candidate best;
    bool ambiguous = false;
    for (const auto &entry : ProxorGui::profileManager->profiles) {
        const std::shared_ptr<ProxorGui::ProxyEntity> &profile = entry.second;
        if (profile == nullptr) continue;

        Candidate candidate;
        candidate.profile = profile;

        if (!dataStore->ssid_on_demand_profile_type.isEmpty() &&
            profile->type == dataStore->ssid_on_demand_profile_type) {
            candidate.matchedType = true;
            candidate.score += 4;
        }
        if (!dataStore->ssid_on_demand_profile_address.isEmpty() &&
            profile->summary_serverAddress.compare(dataStore->ssid_on_demand_profile_address, Qt::CaseInsensitive) == 0) {
            candidate.matchedAddress = true;
            candidate.score += 4;
        }
        if (dataStore->ssid_on_demand_profile_port > 0 &&
            profile->summary_serverPort == dataStore->ssid_on_demand_profile_port) {
            candidate.matchedPort = true;
            candidate.score += 3;
        }
        if (!dataStore->ssid_on_demand_profile_name.isEmpty() &&
            profile->summary_name == dataStore->ssid_on_demand_profile_name) {
            candidate.matchedName = true;
            candidate.score += 2;
        }

        const bool acceptable =
            (candidate.matchedAddress && candidate.matchedType) ||
            (candidate.matchedAddress && candidate.matchedPort) ||
            (candidate.matchedType && candidate.matchedName && candidate.matchedPort);
        if (!acceptable) continue;

        if (candidate.score > best.score) {
            best = candidate;
            ambiguous = false;
        } else if (candidate.score > 0 && candidate.score == best.score) {
            ambiguous = true;
        }
    }

    if (ambiguous || best.profile == nullptr) return nullptr;
    return best.profile;
}

void storeOnDemandProfileRef(const std::shared_ptr<ProxorGui::ProxyEntity> &profile) {
    auto *dataStore = ProxorGui::dataStore;
    if (profile == nullptr) {
        dataStore->ssid_on_demand_profile_id = -1919;
        dataStore->ssid_on_demand_profile_name.clear();
        dataStore->ssid_on_demand_profile_type.clear();
        dataStore->ssid_on_demand_profile_address.clear();
        dataStore->ssid_on_demand_profile_port = 0;
        return;
    }

    dataStore->ssid_on_demand_profile_id = profile->id;
    dataStore->ssid_on_demand_profile_name = profile->summary_name;
    dataStore->ssid_on_demand_profile_type = profile->type;
    dataStore->ssid_on_demand_profile_address = profile->summary_serverAddress;
    dataStore->ssid_on_demand_profile_port = profile->summary_serverPort;
}

} // namespace

DialogSSIDSettings::DialogSSIDSettings(QWidget *parent) : QDialog(parent), ui(new Ui::DialogSSIDSettings) {
    ui->setupUi(this);
    ADD_ASTERISK(this);
    D_LOAD_BOOL(ssid_on_demand_enabled);
    populateProfileCombo();
    for (const auto &ssid : ProxorGui::dataStore->ssid_trigger_list) {
        ui->ssid_list_widget->addItem(ssid);
    }
    const std::shared_ptr<ProxorGui::ProxyEntity> savedProfile = resolveSavedOnDemandProfile();
    if (savedProfile != nullptr) {
        const int index = ui->ssid_on_demand_profile->findData(savedProfile->id);
        if (index >= 0) {
            ui->ssid_on_demand_profile->setCurrentIndex(index);
        }
    }
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &DialogSSIDSettings::accept);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    const auto ssidCapability = ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::OnDemandSsid);
    if (ssidCapability.support != ProxorPlatform::Support::Supported) {
        auto *note = ProxorPlatform::MakeCapabilityNote(this);
        ui->verticalLayout->insertWidget(0, note);
        ProxorPlatform::ApplyCapability({ui->groupBox_activation, ui->groupBox_ssids}, ssidCapability, note);
    }

    // Live Wi-Fi status: never passed to ApplyCapability, so the user always sees why On-Demand cannot work.
    auto *wifiStatusGroup = new QGroupBox(tr("Wi-Fi status"), this);
    wifiStatusGroup->setObjectName("wifi_status_group");
    auto *statusLayout = new QVBoxLayout(wifiStatusGroup);
    m_wifiStatus = new QLabel(wifiStatusGroup);
    m_wifiStatus->setObjectName("wifi_status_label");
    m_wifiStatus->setWordWrap(true);
    m_wifiStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLayout->addWidget(m_wifiStatus);

    auto *row = new QHBoxLayout();
    m_addCurrent = new QPushButton(tr("Add Current Network"), wifiStatusGroup);
    m_addCurrent->setObjectName("btn_add_current_ssid");
    m_refreshWifi = new QPushButton(tr("Refresh"), wifiStatusGroup);
    m_refreshWifi->setObjectName("btn_refresh_wifi");
    row->addWidget(m_addCurrent);
    row->addWidget(m_refreshWifi);
    row->addStretch();
    statusLayout->addLayout(row);

    m_permissionNote = new QLabel(wifiStatusGroup);
    m_permissionNote->setObjectName("wifi_permission_label");
    m_permissionNote->setWordWrap(true);
    m_permissionButton = new QPushButton(wifiStatusGroup);
    m_permissionButton->setObjectName("btn_wifi_permission");
    m_permissionNote->hide();
    m_permissionButton->hide();
    statusLayout->addWidget(m_permissionNote);
    statusLayout->addWidget(m_permissionButton, 0, Qt::AlignLeft);

    auto *hint = new QLabel(tr("Proxor reads the Wi-Fi network only while On-Demand is on with at least one network, or a Hosts entry has \"Skip on SSIDs\"."), wifiStatusGroup);
    hint->setWordWrap(true);
    QPalette hintPalette = hint->palette();
    hintPalette.setColor(QPalette::WindowText, hintPalette.color(QPalette::PlaceholderText));
    hint->setPalette(hintPalette);
    statusLayout->addWidget(hint);

    ui->verticalLayout->insertWidget(ssidCapability.support != ProxorPlatform::Support::Supported ? 1 : 0, wifiStatusGroup);

    connect(m_addCurrent, &QPushButton::clicked, this, [this] {
        auto *m = WifiMonitor::appInstance();
        if (m == nullptr) return;
        const auto reading = m->lastReading();
        if (reading.state != ProxorWifi::ReadState::Connected || reading.ssid.isEmpty()) return;
        for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
            if (ui->ssid_list_widget->item(i)->text() == reading.ssid) return;
        }
        ui->ssid_list_widget->addItem(reading.ssid);
        updateAddCurrentEnabled();
    });
    connect(m_permissionButton, &QPushButton::clicked, this, [this] {
        const auto perm = ProxorWifi::CurrentWifiPermission();
        if (perm == ProxorWifi::PermissionState::NotDetermined) {
            ProxorWifi::RequestWifiPermission(this, [this](ProxorWifi::PermissionState) {
                updatePermissionRow();
                if (auto *m = WifiMonitor::appInstance()) m->refreshNow();
            });
        } else if (!ProxorWifi::OpenWifiPermissionSettings()) {
            MessageBoxInfo(windowTitle(), ProxorWifi::DescribePermission(perm));
        }
    });

    if (auto *m = WifiMonitor::appInstance()) {
        m_wifiStatus->setText(m->hasReading() ? ProxorWifi::DescribeReading(m->lastReading())
                                              : tr("Checking the Wi-Fi network..."));
        connect(m, &WifiMonitor::readingChanged, this, &DialogSSIDSettings::updateWifiStatus);
        connect(m_refreshWifi, &QPushButton::clicked, m, &WifiMonitor::refreshNow);
        m->refreshNow();
    } else {
        m_wifiStatus->setText(tr("The Wi-Fi monitor is not running."));
        m_refreshWifi->setEnabled(false);
    }
    updateAddCurrentEnabled();
    updatePermissionRow();
}

DialogSSIDSettings::~DialogSSIDSettings() {
    delete ui;
}

void DialogSSIDSettings::populateProfileCombo() {
    ui->ssid_on_demand_profile->clear();
    ui->ssid_on_demand_profile->addItem(tr("Select a profile"), -1);

    for (auto gid : ProxorGui::profileManager->groupsTabOrder) {
        const auto group = ProxorGui::profileManager->GetGroup(gid);
        if (group == nullptr || group->archive) continue;

        for (const auto &profile : group->ProfilesWithOrder()) {
            if (profile == nullptr) continue;
            const auto label = QStringLiteral("%1 / %2")
                                   .arg(group->name, profile->DisplayTypeAndNameSummary());
            ui->ssid_on_demand_profile->addItem(label, profile->id);
        }
    }
}

void DialogSSIDSettings::updateWifiStatus(const ProxorWifi::WifiReading &reading) {
    m_wifiStatus->setText(ProxorWifi::DescribeReading(reading));
    updateAddCurrentEnabled();
    updatePermissionRow();
}

void DialogSSIDSettings::updateAddCurrentEnabled() {
    bool enabled = false;
    if (auto *m = WifiMonitor::appInstance()) {
        const auto reading = m->lastReading();
        if (m->hasReading() && reading.state == ProxorWifi::ReadState::Connected && !reading.ssid.isEmpty()) {
            enabled = true;
            for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
                if (ui->ssid_list_widget->item(i)->text() == reading.ssid) {
                    enabled = false;
                    break;
                }
            }
        }
    }
    m_addCurrent->setEnabled(enabled);
}

void DialogSSIDSettings::updatePermissionRow() {
    const auto perm = ProxorWifi::CurrentWifiPermission();
    if (perm == ProxorWifi::PermissionState::NotRequired || perm == ProxorWifi::PermissionState::Granted) {
        m_permissionNote->hide();
        m_permissionButton->hide();
        return;
    }
    m_permissionNote->setText(ProxorWifi::DescribePermission(perm));
    m_permissionButton->setText(perm == ProxorWifi::PermissionState::NotDetermined ? tr("Allow Location Access...")
                                                                                   : tr("Open Location Settings..."));
    m_permissionNote->show();
    m_permissionButton->show();
}

void DialogSSIDSettings::on_btn_add_ssid_clicked() {
    auto ssid = ui->ssid_input->text().trimmed();
    if (ssid.isEmpty()) return;
    for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
        if (ui->ssid_list_widget->item(i)->text() == ssid) return;
    }
    ui->ssid_list_widget->addItem(ssid);
    ui->ssid_input->clear();
    updateAddCurrentEnabled();
}

void DialogSSIDSettings::on_btn_remove_ssid_clicked() {
    auto items = ui->ssid_list_widget->selectedItems();
    for (auto *item : items) { delete item; }
    updateAddCurrentEnabled();
}

void DialogSSIDSettings::accept() {
    QStringList flags;
    if (!save(flags)) return;
    ProxorGui::dataStore->Save();
    QDialog::accept();
}

bool DialogSSIDSettings::save(QStringList &flags) {
    Q_UNUSED(flags)
    D_SAVE_BOOL(ssid_on_demand_enabled);
    const int selectedProfileId = ui->ssid_on_demand_profile->currentData().toInt();
    std::shared_ptr<ProxorGui::ProxyEntity> selectedProfile =
        ProxorGui::profileManager->GetProfile(selectedProfileId);
    if (ProxorGui::dataStore->ssid_on_demand_enabled && selectedProfile == nullptr) {
        MessageBoxWarning(windowTitle(), tr("Select a target profile for WiFi on-demand before enabling it."));
        return false;
    }

    QStringList list;
    for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
        list << ui->ssid_list_widget->item(i)->text();
    }
    ProxorGui::dataStore->ssid_trigger_list = list;
    storeOnDemandProfileRef(selectedProfile);
    return true;
}
