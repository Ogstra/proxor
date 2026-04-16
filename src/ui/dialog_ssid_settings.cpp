#include "dialog_ssid_settings.h"
#include "ui_dialog_ssid_settings.h"

#include "db/Database.hpp"
#include "main/GuiUtils.hpp"
#include "main/ProxorGui.hpp"

#include <memory>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>

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
    D_SAVE_BOOL(ssid_on_demand_enabled);
    const int selectedProfileId = ui->ssid_on_demand_profile->currentData().toInt();
    std::shared_ptr<ProxorGui::ProxyEntity> selectedProfile =
        ProxorGui::profileManager->GetProfile(selectedProfileId);
    if (ProxorGui::dataStore->ssid_on_demand_enabled && selectedProfile == nullptr) {
        MessageBoxWarning(windowTitle(), tr("Select a target profile for WiFi on-demand before enabling it."));
        return;
    }

    QStringList list;
    for (int i = 0; i < ui->ssid_list_widget->count(); i++) {
        list << ui->ssid_list_widget->item(i)->text();
    }
    ProxorGui::dataStore->ssid_trigger_list = list;
    storeOnDemandProfileRef(selectedProfile);
    ProxorGui::dataStore->Save();
    QDialog::accept();
}
