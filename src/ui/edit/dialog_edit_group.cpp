#include "dialog_edit_group.h"
#include "ui_dialog_edit_group.h"

#include "db/Database.hpp"
#include "ui/mainwindow_interface.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QRegularExpressionValidator>
#include <QScrollBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#define ADJUST_SIZE runOnUiThread([=] { adjustSize(); adjustPosition(mainwindow); }, this);

namespace {
QString FormatIntervalHours(int minutes) {
    if (minutes <= 0) return {};
    const double hours = minutes / 60.0;
    QString text = QString::number(hours, 'f', hours < 1.0 ? 1 : (minutes % 60 == 0 ? 0 : 1));
    while (text.contains('.') && text.endsWith('0')) text.chop(1);
    if (text.endsWith('.')) text.chop(1);
    return text;
}

int ParseIntervalHoursToMinutes(QString text) {
    text = text.trimmed().replace(',', '.');
    bool ok = false;
    const double hours = text.toDouble(&ok);
    if (!ok || hours <= 0) return 0;
    return static_cast<int>(hours * 60.0 + 0.5);
}

void RefreshUpdateIntervalUi(Ui::DialogEditGroup *ui, bool usingGlobalInterval, int effectiveIntervalMinutes) {
    ui->label_update_interval->setText(QObject::tr("Profile update interval (hours)"));
    if (usingGlobalInterval) {
        ui->sub_update_interval->setPlaceholderText(QObject::tr("Using global interval"));
        ui->sub_update_interval->setToolTip(QObject::tr("This subscription does not define its own interval. The displayed value comes from the global automatic update interval."));
    } else if (effectiveIntervalMinutes > 0) {
        ui->label_update_interval->setText(QObject::tr("Profile update interval (hours)"));
        ui->sub_update_interval->setPlaceholderText(QObject::tr("Provided by subscription"));
        ui->sub_update_interval->setToolTip(QObject::tr("This value comes from the subscription and replaces the global automatic update interval for this subscription."));
    } else {
        ui->sub_update_interval->setPlaceholderText(QObject::tr("Global interval"));
        ui->sub_update_interval->setToolTip(QObject::tr("When the subscription sends profile-update-interval, that value replaces the global automatic update interval for this subscription."));
    }
}

class FrontProxyPickerDialog final : public QDialog {
public:
    explicit FrontProxyPickerDialog(int currentProfileId, QWidget *parent = nullptr)
        : QDialog(parent), m_currentProfileId(currentProfileId) {
        setWindowTitle(QObject::tr("Select Front Proxy"));
        resize(448, 288);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(8);
        m_tabs = new QTabWidget(this);
        m_tabs->setDocumentMode(true);
        layout->addWidget(m_tabs);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
        m_noneButton = buttonBox->addButton(QObject::tr("None"), QDialogButtonBox::ResetRole);
        m_selectButton = buttonBox->addButton(QObject::tr("Select"), QDialogButtonBox::AcceptRole);
        m_selectButton->setEnabled(false);
        layout->addWidget(buttonBox);

        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(m_noneButton, &QPushButton::clicked, this, [this] {
            m_selectedProfileId = -1;
            accept();
        });
        connect(m_selectButton, &QPushButton::clicked, this, [this] {
            auto *table = currentTable();
            if (table == nullptr) return;
            const auto items = table->selectedItems();
            if (items.isEmpty()) return;
            m_selectedProfileId = items.first()->data(Qt::UserRole).toInt();
            accept();
        });
        connect(m_tabs, &QTabWidget::currentChanged, this, [this](int) {
            m_selectButton->setEnabled(currentTable() != nullptr && currentTable()->currentRow() >= 0);
        });

        buildTabs();
    }

    [[nodiscard]] int selectedProfileId() const {
        return m_selectedProfileId;
    }

private:
    int m_currentProfileId = -1;
    int m_selectedProfileId = -114514;
    QTabWidget *m_tabs = nullptr;
    QPushButton *m_noneButton = nullptr;
    QPushButton *m_selectButton = nullptr;

    [[nodiscard]] QTableWidget *currentTable() const {
        return qobject_cast<QTableWidget *>(m_tabs->currentWidget());
    }

    void buildTabs() {
        for (const auto groupId: ProxorGui::profileManager->groupsTabOrder) {
            const auto group = ProxorGui::profileManager->GetGroup(groupId);
            if (group == nullptr || group->archive) continue;

            auto *table = new QTableWidget(this);
            table->setColumnCount(3);
            table->setHorizontalHeaderLabels({
                QObject::tr("Name"),
                QObject::tr("Type"),
                QObject::tr("Address"),
            });
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->setEditTriggers(QAbstractItemView::NoEditTriggers);
            table->setAlternatingRowColors(true);
            table->setContentsMargins(0, 0, 0, 0);
            table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            table->verticalHeader()->setVisible(false);
            table->horizontalHeader()->setMinimumSectionSize(80);
            table->horizontalHeader()->setStretchLastSection(false);
            table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
            table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
            table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

            int selectedRow = -1;
            const auto profiles = group->ProfilesWithOrder();
            table->setRowCount(profiles.size());
            for (int row = 0; row < profiles.size(); ++row) {
                const auto &profile = profiles[row];
                if (profile == nullptr) continue;

                auto *nameItem = new QTableWidgetItem(profile->summary_name);
                nameItem->setData(Qt::UserRole, profile->id);
                auto *typeItem = new QTableWidgetItem(profile->DisplayTypeSummary());
                typeItem->setData(Qt::UserRole, profile->id);
                auto *addrItem = new QTableWidgetItem(profile->DisplayAddressSummary());
                addrItem->setData(Qt::UserRole, profile->id);

                table->setItem(row, 0, nameItem);
                table->setItem(row, 1, typeItem);
                table->setItem(row, 2, addrItem);

                if (profile->id == m_currentProfileId) {
                    selectedRow = row;
                }
            }

            connect(table, &QTableWidget::itemSelectionChanged, this, [this, table] {
                Q_UNUSED(table);
                m_selectButton->setEnabled(currentTable() != nullptr && currentTable()->currentRow() >= 0);
            });
            connect(table, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *item) {
                if (item == nullptr) return;
                m_selectedProfileId = item->data(Qt::UserRole).toInt();
                accept();
            });

            m_tabs->addTab(table, group->name);
            if (selectedRow >= 0) {
                m_tabs->setCurrentWidget(table);
                table->selectRow(selectedRow);
            }
        }

        if (auto *table = currentTable(); table != nullptr && table->currentRow() >= 0) {
            m_selectButton->setEnabled(true);
        }
    }
};
}

DialogEditGroup::DialogEditGroup(const std::shared_ptr<ProxorGui::Group> &ent, QWidget *parent) : QDialog(parent), ui(new Ui::DialogEditGroup) {
    ui->setupUi(this);
    this->ent = ent;

    connect(ui->type, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [=](int index) {
        ui->cat_sub->setHidden(index == 0);
        ADJUST_SIZE
    });
    connect(ui->skip_auto_update, &QCheckBox::toggled, this, [=](bool checked) {
        ui->cat_update->setEnabled(!checked);
    });

    ui->name->setText(ent->name);
    ui->archive->setChecked(ent->archive);
    ui->skip_auto_update->setChecked(ent->skip_auto_update);
    ui->sub_update_interval->setValidator(new QRegularExpressionValidator(QRegularExpression("^[0-9]+([\\.,][0-9]+)?$"), this));
    ui->sub_update_always->setChecked(ent->sub_update_always);
    ui->sub_update_always->setToolTip(tr("When enabled, this subscription updates on startup even if the global startup toggle is disabled."));
    ui->url->setText(ent->url);
    ui->type->setCurrentIndex(ent->url.isEmpty() ? 0 : 1);
    ui->type->currentIndexChanged(ui->type->currentIndex());
    ui->cat_update->setEnabled(!ent->skip_auto_update);
    const int globalUpdateMinutes = std::abs(ProxorGui::dataStore->sub_auto_update);
    const bool useGlobalUpdateInterval = ent->sub_update_interval <= 0 && !ent->skip_auto_update && globalUpdateMinutes > 0;
    const int effectiveUpdateMinutes = ent->sub_update_interval > 0 ? ent->sub_update_interval : (useGlobalUpdateInterval ? globalUpdateMinutes : 0);
    const QString effectiveUpdateText = FormatIntervalHours(effectiveUpdateMinutes);
    ui->sub_update_interval->setText(effectiveUpdateText);
    CACHE.using_global_update_interval = useGlobalUpdateInterval;
    CACHE.initial_update_interval_text = effectiveUpdateText;
    RefreshUpdateIntervalUi(ui, useGlobalUpdateInterval, effectiveUpdateMinutes);
    ui->manually_column_width->setChecked(ent->manually_column_width);
    ui->cat_share->setVisible(false);

    if (ent->id >= 0) { // already a group
        ui->type->setDisabled(true);
        if (!ent->Profiles().isEmpty()) {
            ui->cat_share->setVisible(true);
        }
    } else { // new group
        ui->front_proxy->hide();
        ui->front_proxy_l->hide();
        ui->front_proxy_clear->hide();
    }

    CACHE.front_proxy = ent->front_proxy_id;
    refresh_front_proxy();
    connect(ui->front_proxy_clear, &QPushButton::clicked, this, [=] {
        CACHE.front_proxy = -1;
        refresh_front_proxy();
    });

    connect(ui->copy_links, &QPushButton::clicked, this, [=] {
        QStringList links;
        for (const auto &profile: ent->Profiles()) {
            if (!profile->EnsureHydrated()) continue;
            links += profile->bean->ToShareLink();
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });
    connect(ui->copy_links_nkr, &QPushButton::clicked, this, [=] {
        QStringList links;
        for (const auto &profile: ent->Profiles()) {
            if (!profile->EnsureHydrated()) continue;
            links += profile->bean->ToProxorShareLink(profile->type);
        }
        QApplication::clipboard()->setText(links.join("\n"));
        MessageBoxInfo(software_name, tr("Copied"));
    });

    ADJUST_SIZE
}

DialogEditGroup::~DialogEditGroup() {
    delete ui;
}

void DialogEditGroup::accept() {
    if (ent->id >= 0) { // already a group
        if (!ent->url.isEmpty() && ui->url->text().isEmpty()) {
            MessageBoxWarning(tr("Warning"), tr("Please input URL"));
            return;
        }
    }
    ent->name = ui->name->text();
    ent->url = ui->url->text();
    ent->archive = ui->archive->isChecked();
    ent->skip_auto_update = ui->skip_auto_update->isChecked();
    const auto updateIntervalText = ui->sub_update_interval->text().trimmed();
    if (CACHE.using_global_update_interval && updateIntervalText == CACHE.initial_update_interval_text) {
        ent->sub_update_interval = 0;
    } else {
        ent->sub_update_interval = ParseIntervalHoursToMinutes(updateIntervalText);
    }
    ent->sub_update_always = ui->sub_update_always->isChecked();
    ent->manually_column_width = ui->manually_column_width->isChecked();
    ent->front_proxy_id = CACHE.front_proxy;
    QDialog::accept();
}

void DialogEditGroup::refresh_front_proxy() {
    auto fEnt = ProxorGui::profileManager->GetProfile(CACHE.front_proxy);
    ui->front_proxy->setText(fEnt == nullptr ? tr("None") : fEnt->bean->DisplayTypeAndName());
}

void DialogEditGroup::on_front_proxy_clicked() {
    FrontProxyPickerDialog dialog(CACHE.front_proxy, this);
    if (dialog.exec() != QDialog::Accepted) return;
    CACHE.front_proxy = dialog.selectedProfileId();
    refresh_front_proxy();
}
