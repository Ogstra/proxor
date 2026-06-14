#include "dialog_manage_routes.h"
#include "ui_dialog_manage_routes.h"

#include "3rdparty/qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "3rdparty/qv2ray/v3/components/GeositeReader/GeositeReader.hpp"
#include "main/GuiUtils.hpp"
#include "fmt/Preset.hpp"
#include "ui/ThemeManager.hpp"

#include <QFile>
#include <QMessageBox>
#include <QListWidget>
#include <QLineEdit>
#include <QHeaderView>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QPlainTextEdit>
#include <QLabel>
#include <QTreeWidget>
#include <QDialogButtonBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#define REFRESH_ACTIVE_ROUTING(name, obj)           \
    this->active_routing = name;                    \
    setWindowTitle(title_base + " [" + name + "]"); \
    UpdateDisplayRouting(obj, false);

namespace {
QString SettingsListStyleForTheme(const QString &themeName) {
    QString style = QStringLiteral("QListWidget::item{padding:4px 10px;}");
    if (themeManager->NormalizeTheme(themeName) != QStringLiteral("System")) {
        style += QStringLiteral(
            "QListWidget::item:selected{background:#455364;color:#DFE1E2;}"
            "QListWidget::item:selected:active{background:#455364;color:#DFE1E2;}"
            "QListWidget::item:selected:!active{background:#455364;color:#DFE1E2;}"
        );
    }
    return style;
}

enum DirectSiteTargetRole {
    TargetKindRole = Qt::UserRole,
    TargetIdRole = Qt::UserRole + 1,
};
enum DirectSiteTargetKind { TargetGroup = 0, TargetProfile = 1 };

// Lets the user pick which subscription groups (whole) and/or individual profiles a
// direct-site rule applies to. Group node checked => whole subscription; child checked => that profile.
class TargetPickerDialog final : public QDialog {
public:
    TargetPickerDialog(const QList<int> &groups, const QList<int> &profiles, QWidget *parent = nullptr)
        : QDialog(parent) {
        setWindowTitle(QObject::tr("Select Targets"));
        resize(420, 460);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(8);

        m_tree = new QTreeWidget(this);
        m_tree->setHeaderHidden(true);
        m_tree->setUniformRowHeights(true);
        layout->addWidget(m_tree);

        auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Ok, this);
        layout->addWidget(buttonBox);
        connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

        const auto groupSet = QSet<int>(groups.begin(), groups.end());
        const auto profileSet = QSet<int>(profiles.begin(), profiles.end());
        for (const auto gid: ProxorGui::profileManager->groupsTabOrder) {
            const auto group = ProxorGui::profileManager->GetGroup(gid);
            if (group == nullptr || group->archive) continue;

            auto *groupItem = new QTreeWidgetItem(m_tree);
            groupItem->setText(0, group->name);
            groupItem->setFlags(groupItem->flags() | Qt::ItemIsUserCheckable);
            groupItem->setData(0, TargetKindRole, TargetGroup);
            groupItem->setData(0, TargetIdRole, group->id);
            groupItem->setCheckState(0, groupSet.contains(group->id) ? Qt::Checked : Qt::Unchecked);
            groupItem->setExpanded(true);

            for (const auto &profile: group->ProfilesWithOrder()) {
                if (profile == nullptr) continue;
                auto *profileItem = new QTreeWidgetItem(groupItem);
                profileItem->setText(0, profile->summary_name);
                profileItem->setFlags(profileItem->flags() | Qt::ItemIsUserCheckable);
                profileItem->setData(0, TargetKindRole, TargetProfile);
                profileItem->setData(0, TargetIdRole, profile->id);
                profileItem->setCheckState(0, profileSet.contains(profile->id) ? Qt::Checked : Qt::Unchecked);
            }
        }
    }

    [[nodiscard]] QList<int> selectedGroups() const {
        QList<int> out;
        for (auto *top: topLevelItems()) {
            if (top->checkState(0) == Qt::Checked) out += top->data(0, TargetIdRole).toInt();
        }
        return out;
    }

    [[nodiscard]] QList<int> selectedProfiles() const {
        QList<int> out;
        for (auto *top: topLevelItems()) {
            for (int i = 0; i < top->childCount(); ++i) {
                auto *child = top->child(i);
                if (child->checkState(0) == Qt::Checked) out += child->data(0, TargetIdRole).toInt();
            }
        }
        return out;
    }

private:
    QTreeWidget *m_tree = nullptr;

    [[nodiscard]] QList<QTreeWidgetItem *> topLevelItems() const {
        QList<QTreeWidgetItem *> out;
        for (int i = 0; i < m_tree->topLevelItemCount(); ++i) out += m_tree->topLevelItem(i);
        return out;
    }
};
}

DialogManageRoutes::DialogManageRoutes(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageRoutes) {
    ui->setupUi(this);
    title_base = windowTitle();
    if (auto *vbox = qobject_cast<QVBoxLayout *>(this->layout())) {
        vbox->setContentsMargins(8, 8, 8, 8);
        vbox->setSpacing(8);
    }

    QStringList qsValue = {""};
    //
    ui->outbound_domain_strategy->addItems(Preset::SingBox::DomainStrategy);
    ui->domainStrategyCombo->addItems(Preset::SingBox::DomainStrategy);
    qsValue += QStringLiteral("prefer_ipv4 prefer_ipv6 ipv4_only ipv6_only").split(" ");
    ui->dns_object->setPlaceholderText(DecodeB64IfValid("ewogICJzZXJ2ZXJzIjogW10sCiAgInJ1bGVzIjogW10sCiAgImZpbmFsIjogIiIsCiAgInN0cmF0ZWd5IjogIiIsCiAgImRpc2FibGVfY2FjaGUiOiBmYWxzZSwKICAiZGlzYWJsZV9leHBpcmUiOiBmYWxzZSwKICAiaW5kZXBlbmRlbnRfY2FjaGUiOiBmYWxzZSwKICAicmV2ZXJzZV9tYXBwaW5nIjogZmFsc2UsCiAgImZha2VpcCI6IHt9Cn0="));
    ui->direct_dns_strategy->addItems(qsValue);
    ui->remote_dns_strategy->addItems(qsValue);
    //
    D_C_LOAD_STRING(custom_route_global)
    //
    connect(ui->use_dns_object, &QCheckBox::stateChanged, this, [=](int state) {
        auto useDNSObject = state == Qt::Checked;
        ui->simple_dns_box->setDisabled(useDNSObject);
        ui->dns_object->setDisabled(!useDNSObject);
    });
    ui->use_dns_object->stateChanged(Qt::Unchecked); // uncheck to uncheck
    connect(ui->format_dns_object, &QPushButton::clicked, this, [=] {
        auto obj = QString2QJsonObject(ui->dns_object->toPlainText());
        if (obj.isEmpty()) {
            MessageBoxInfo("DNS", "invaild json");
        } else {
            ui->dns_object->setPlainText(QJsonObject2QString(obj, false));
        }
    });
    //
    connect(ui->custom_route_edit, &QPushButton::clicked, this, [=] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_route)
    });
    connect(ui->custom_route_global_edit, &QPushButton::clicked, this, [=] {
        C_EDIT_JSON_ALLOW_EMPTY(custom_route_global)
    });
    //
    builtInSchemesMenu = new QMenu(this);
    builtInSchemesMenu->addActions(this->getBuiltInSchemes());
    ui->preset->setMenu(builtInSchemesMenu);

    QString geoipFn = ProxorGui::FindCoreAsset("geoip.dat");
    QString geositeFn = ProxorGui::FindCoreAsset("geosite.dat");
    //
    const auto sourceStringsDomain = Qv2ray::components::GeositeReader::ReadGeoSiteFromFile(geositeFn);
    directDomainTxt = new AutoCompleteTextEdit("geosite", sourceStringsDomain, this);
    proxyDomainTxt = new AutoCompleteTextEdit("geosite", sourceStringsDomain, this);
    blockDomainTxt = new AutoCompleteTextEdit("geosite", sourceStringsDomain, this);
    //
    const auto sourceStringsIP = Qv2ray::components::GeositeReader::ReadGeoSiteFromFile(geoipFn);
    directIPTxt = new AutoCompleteTextEdit("geoip", sourceStringsIP, this);
    proxyIPTxt = new AutoCompleteTextEdit("geoip", sourceStringsIP, this);
    blockIPTxt = new AutoCompleteTextEdit("geoip", sourceStringsIP, this);
    //
    ui->directTxtLayout->addWidget(directDomainTxt, 0, 0);
    ui->proxyTxtLayout->addWidget(proxyDomainTxt, 0, 0);
    ui->blockTxtLayout->addWidget(blockDomainTxt, 0, 0);
    //
    ui->directIPLayout->addWidget(directIPTxt, 0, 0);
    ui->proxyIPLayout->addWidget(proxyIPTxt, 0, 0);
    ui->blockIPLayout->addWidget(blockIPTxt, 0, 0);
    const auto prepareRuleEditor = [](QPlainTextEdit *editor, const QString &placeholder) {
        editor->setMinimumHeight(82);
        editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        editor->setPlaceholderText(placeholder);
        editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    };
    prepareRuleEditor(directIPTxt, tr("geoip:private\ngeoip:cn"));
    prepareRuleEditor(proxyIPTxt, tr("geoip:telegram"));
    prepareRuleEditor(blockIPTxt, tr("geoip:private"));
    prepareRuleEditor(directDomainTxt, tr("geosite:private\ngeosite:cn"));
    prepareRuleEditor(proxyDomainTxt, tr("geosite:geolocation-!cn"));
    prepareRuleEditor(blockDomainTxt, tr("geosite:category-ads-all"));
    for (auto *box: {ui->directIpBox, ui->proxyIpBox, ui->blockIpBox,
                     ui->directDomainBox, ui->proxyDomainBox, ui->blockDomainBox}) {
        box->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    }
    //
    hostsMapTable = new QTableWidget(0, 3, this);
    hostsMapTable->setHorizontalHeaderLabels({tr("Hostname"), tr("IP"), tr("Skip on SSIDs")});
    hostsMapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    hostsMapTable->horizontalHeaderItem(2)->setToolTip(tr("Comma-separated SSIDs where this entry is NOT applied (skipped). Useful for using local DNS when on a known home WiFi."));
    hostsMapTable->verticalHeader()->setVisible(false);
    hostsMapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    auto addHostBtn = new QPushButton(tr("Add"), this);
    auto removeHostBtn = new QPushButton(tr("Remove"), this);
    connect(addHostBtn, &QPushButton::clicked, this, [this] {
        const int row = hostsMapTable->rowCount();
        hostsMapTable->insertRow(row);
        hostsMapTable->setItem(row, 0, new QTableWidgetItem(""));
        hostsMapTable->setItem(row, 1, new QTableWidgetItem(""));
        hostsMapTable->setItem(row, 2, new QTableWidgetItem(""));
        hostsMapTable->editItem(hostsMapTable->item(row, 0));
    });
    connect(removeHostBtn, &QPushButton::clicked, this, [this] {
        const auto rows = hostsMapTable->selectionModel()->selectedRows();
        QList<int> rowIndexes;
        for (const auto &idx: rows) rowIndexes << idx.row();
        std::sort(rowIndexes.begin(), rowIndexes.end(), std::greater<int>());
        for (int r: rowIndexes) hostsMapTable->removeRow(r);
    });
    ui->hostsMapLayout->addWidget(hostsMapTable, 0, 0, 1, 2);
    ui->hostsMapLayout->addWidget(addHostBtn, 1, 0);
    ui->hostsMapLayout->addWidget(removeHostBtn, 1, 1);
    //
    buildDirectSitesTab();
    //
    REFRESH_ACTIVE_ROUTING(ProxorGui::dataStore->active_routing, ProxorGui::dataStore->routing.get())

    ADD_ASTERISK(this)
    wrapTabPagesInScrollAreas();

    ui->tabWidget->tabBar()->hide();
    ui->tabWidget->setDocumentMode(true);
    auto *routeNav = new QListWidget(this);
    for (int i = 0; i < ui->tabWidget->count(); ++i) {
        routeNav->addItem(ui->tabWidget->tabText(i));
    }
    routeNav->setCurrentRow(ui->tabWidget->currentIndex());
    routeNav->setFrameShape(QFrame::NoFrame);
    routeNav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    routeNav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    routeNav->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    routeNav->setStyleSheet(SettingsListStyleForTheme(ProxorGui::dataStore->theme));
    routeNav->setFixedWidth(routeNav->sizeHintForColumn(0) + 32);
    connect(routeNav, &QListWidget::currentRowChanged, ui->tabWidget, &QTabWidget::setCurrentIndex);
    connect(themeManager, &ThemeManager::themeChanged, routeNav, [routeNav](const QString &themeName) {
        routeNav->setStyleSheet(SettingsListStyleForTheme(themeName));
    });
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int) {
        updateGeometry();
        emit activePageGeometryChanged();
    });
    if (auto *vbox = qobject_cast<QVBoxLayout *>(this->layout())) {
        const int idx = vbox->indexOf(ui->tabWidget);
        auto *navRow = new QHBoxLayout();
        navRow->setContentsMargins(0, 0, 0, 0);
        navRow->setSpacing(8);
        navRow->addWidget(routeNav);
        navRow->addWidget(ui->tabWidget, 1);
        vbox->insertLayout(idx, navRow);
    }
}

DialogManageRoutes::~DialogManageRoutes() {
    delete ui;
}

void DialogManageRoutes::wrapTabPagesInScrollAreas() {
    const int currentIndex = ui->tabWidget->currentIndex();
    const int count = ui->tabWidget->count();
    for (int i = 0; i < count; ++i) {
        auto *page = ui->tabWidget->widget(i);
        if (qobject_cast<QScrollArea *>(page) != nullptr) continue;

        const auto title = ui->tabWidget->tabText(i);
        const auto icon = ui->tabWidget->tabIcon(i);
        const auto tooltip = ui->tabWidget->tabToolTip(i);
        const auto whatsThis = ui->tabWidget->tabWhatsThis(i);
        const bool enabled = ui->tabWidget->isTabEnabled(i);

        auto *scroll = new QScrollArea(ui->tabWidget);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        page->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        ui->tabWidget->removeTab(i);
        scroll->setWidget(page);
        ui->tabWidget->insertTab(i, scroll, icon, title);
        ui->tabWidget->setTabToolTip(i, tooltip);
        ui->tabWidget->setTabWhatsThis(i, whatsThis);
        ui->tabWidget->setTabEnabled(i, enabled);
    }
    ui->tabWidget->setCurrentIndex(currentIndex);
}

int DialogManageRoutes::activePageHeightHint() const {
    auto *current = ui->tabWidget->currentWidget();
    int height = 0;
    if (auto *scroll = qobject_cast<QScrollArea *>(current)) {
        if (auto *page = scroll->widget()) {
            height = qMax(page->sizeHint().height(), page->minimumSizeHint().height());
        }
    } else if (current != nullptr) {
        height = qMax(current->sizeHint().height(), current->minimumSizeHint().height());
    }

    if (height <= 0) return sizeHint().height();
    if (auto *layout = this->layout()) {
        const auto margins = layout->contentsMargins();
        height += margins.top() + margins.bottom();
        if (ui->buttonBox->isVisible()) {
            height += layout->spacing() + ui->buttonBox->sizeHint().height();
        }
    }
    return height;
}

void DialogManageRoutes::accept() {
    QStringList flags{"UpdateDataStore"};
    if (!save(flags)) return;
    MW_dialog_message(Dialog_DialogManageRoutes, flags.join(""));
    QDialog::accept();
}

bool DialogManageRoutes::save(QStringList &flags) {
    D_C_SAVE_STRING(custom_route_global)
    bool routeChanged = false;
    commitDirectSitesEditor();
    const auto directRulesJson = serializedDirectSiteRules();
    if (ProxorGui::dataStore->direct_site_rules != directRulesJson) {
        ProxorGui::dataStore->direct_site_rules = directRulesJson;
        routeChanged = true;
    }
    if (ProxorGui::dataStore->active_routing != active_routing) routeChanged = true;
    SaveDisplayRouting(ProxorGui::dataStore->routing.get());
    ProxorGui::dataStore->active_routing = active_routing;
    ProxorGui::dataStore->routing->fn = ROUTES_PREFIX + ProxorGui::dataStore->active_routing;
    if (ProxorGui::dataStore->routing->Save()) routeChanged = true;
    if (routeChanged) flags << "RouteChanged";
    return true;
}

// built in settings

QList<QAction *> DialogManageRoutes::getBuiltInSchemes() {
    QList<QAction *> list;
    list.append(this->schemeToAction(tr("Bypass LAN and China"), routing_cn_lan));
    list.append(this->schemeToAction(tr("Global"), routing_global));
    return list;
}

QAction *DialogManageRoutes::schemeToAction(const QString &name, const ProxorGui::Routing &scheme) {
    auto *action = new QAction(name, this);
    connect(action, &QAction::triggered, [this, &scheme] { this->UpdateDisplayRouting((ProxorGui::Routing *) &scheme, true); });
    return action;
}

void DialogManageRoutes::UpdateDisplayRouting(ProxorGui::Routing *conf, bool qv) {
    //
    directDomainTxt->setPlainText(conf->direct_domain);
    proxyDomainTxt->setPlainText(conf->proxy_domain);
    blockDomainTxt->setPlainText(conf->block_domain);
    //
    blockIPTxt->setPlainText(conf->block_ip);
    directIPTxt->setPlainText(conf->direct_ip);
    proxyIPTxt->setPlainText(conf->proxy_ip);
    //
    CACHE.custom_route = conf->custom;
    ui->def_outbound->setCurrentText(conf->def_outbound);
    //
    if (qv) return;
    //
    ui->sniffing_mode->setCurrentIndex(conf->sniffing_mode);
    ui->outbound_domain_strategy->setCurrentText(conf->outbound_domain_strategy);
    ui->domainStrategyCombo->setCurrentText(conf->domain_strategy);
    ui->use_dns_object->setChecked(conf->use_dns_object);
    ui->dns_object->setPlainText(conf->dns_object);
    ui->dns_routing->setChecked(conf->dns_routing);
    ui->remote_dns->setCurrentText(conf->remote_dns);
    ui->remote_dns_strategy->setCurrentText(conf->remote_dns_strategy);
    ui->direct_dns->setCurrentText(conf->direct_dns);
    ui->direct_dns_strategy->setCurrentText(conf->direct_dns_strategy);
    ui->dns_final_out->setCurrentText(conf->dns_final_out);
    //
    hostsMapTable->setRowCount(0);
    for (const auto &line: SplitLinesSkipSharp(conf->hosts_mapping)) {
        const auto parts = line.simplified().split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;
        const int row = hostsMapTable->rowCount();
        hostsMapTable->insertRow(row);
        hostsMapTable->setItem(row, 0, new QTableWidgetItem(parts[0]));
        hostsMapTable->setItem(row, 1, new QTableWidgetItem(parts[1]));
        hostsMapTable->setItem(row, 2, new QTableWidgetItem(parts.size() >= 3 ? parts[2] : ""));
    }
}

void DialogManageRoutes::SaveDisplayRouting(ProxorGui::Routing *conf) {
    conf->direct_ip = directIPTxt->toPlainText();
    conf->direct_domain = directDomainTxt->toPlainText();
    conf->proxy_ip = proxyIPTxt->toPlainText();
    conf->proxy_domain = proxyDomainTxt->toPlainText();
    conf->block_ip = blockIPTxt->toPlainText();
    conf->block_domain = blockDomainTxt->toPlainText();
    conf->def_outbound = ui->def_outbound->currentText();
    conf->custom = CACHE.custom_route;
    //
    conf->sniffing_mode = ui->sniffing_mode->currentIndex();
    conf->domain_strategy = ui->domainStrategyCombo->currentText();
    conf->outbound_domain_strategy = ui->outbound_domain_strategy->currentText();
    conf->use_dns_object = ui->use_dns_object->isChecked();
    conf->dns_object = ui->dns_object->toPlainText();
    conf->dns_routing = ui->dns_routing->isChecked();
    conf->remote_dns = ui->remote_dns->currentText();
    conf->remote_dns_strategy = ui->remote_dns_strategy->currentText();
    conf->direct_dns = ui->direct_dns->currentText();
    conf->direct_dns_strategy = ui->direct_dns_strategy->currentText();
    conf->dns_final_out = ui->dns_final_out->currentText();
    //
    QStringList hostsLines;
    for (int r = 0; r < hostsMapTable->rowCount(); ++r) {
        const auto hostItem = hostsMapTable->item(r, 0);
        const auto ipItem = hostsMapTable->item(r, 1);
        const auto skipItem = hostsMapTable->item(r, 2);
        if (!hostItem || !ipItem) continue;
        const auto host = hostItem->text().trimmed();
        const auto ip = ipItem->text().trimmed();
        if (host.isEmpty() || ip.isEmpty()) continue;
        QString line = host + " " + ip;
        if (skipItem) {
            // Normalize: remove spaces around commas so it stays single token.
            const auto skipNorm = skipItem->text().trimmed().remove(' ');
            if (!skipNorm.isEmpty()) line += " " + skipNorm;
        }
        hostsLines << line;
    }
    conf->hosts_mapping = hostsLines.join("\n");
}

void DialogManageRoutes::on_load_save_clicked() {
    auto w = new QDialog;
    auto layout = new QVBoxLayout;
    w->setLayout(layout);
    auto lineEdit = new QLineEdit;
    layout->addWidget(lineEdit);
    auto list = new QListWidget;
    layout->addWidget(list);
    for (const auto &name: ProxorGui::Routing::List()) {
        list->addItem(name);
    }
    connect(list, &QListWidget::currentTextChanged, lineEdit, &QLineEdit::setText);
    auto bottom = new QHBoxLayout;
    layout->addLayout(bottom);
    auto load = new QPushButton;
    load->setText(tr("Load"));
    bottom->addWidget(load);
    auto save = new QPushButton;
    save->setText(tr("Save"));
    bottom->addWidget(save);
    auto remove = new QPushButton;
    remove->setText(tr("Remove"));
    bottom->addWidget(remove);
    auto cancel = new QPushButton;
    cancel->setText(tr("Cancel"));
    bottom->addWidget(cancel);
    connect(load, &QPushButton::clicked, w, [=] {
        auto fn = lineEdit->text();
        if (!fn.isEmpty()) {
            auto r = std::make_unique<ProxorGui::Routing>();
            r->load_control_must = true;
            r->fn = ROUTES_PREFIX + fn;
            if (r->Load()) {
                if (QMessageBox::question(nullptr, software_name, tr("Load routing: %1").arg(fn) + "\n" + r->DisplayRouting()) == QMessageBox::Yes) {
                    REFRESH_ACTIVE_ROUTING(fn, r.get()) // temp save to the window
                    w->accept();
                }
            }
        }
    });
    connect(save, &QPushButton::clicked, w, [=] {
        auto fn = lineEdit->text();
        if (!fn.isEmpty()) {
            auto r = std::make_unique<ProxorGui::Routing>();
            SaveDisplayRouting(r.get());
            r->fn = ROUTES_PREFIX + fn;
            if (QMessageBox::question(nullptr, software_name, tr("Save routing: %1").arg(fn) + "\n" + r->DisplayRouting()) == QMessageBox::Yes) {
                r->Save();
                REFRESH_ACTIVE_ROUTING(fn, r.get())
                w->accept();
            }
        }
    });
    connect(remove, &QPushButton::clicked, w, [=] {
        auto fn = lineEdit->text();
        if (!fn.isEmpty() && ProxorGui::Routing::List().length() > 1) {
            if (QMessageBox::question(nullptr, software_name, tr("Remove routing: %1").arg(fn)) == QMessageBox::Yes) {
                QFile f(ROUTES_PREFIX + fn);
                f.remove();
                if (ProxorGui::dataStore->active_routing == fn) {
                    ProxorGui::Routing::SetToActive(ProxorGui::Routing::List().first());
                    REFRESH_ACTIVE_ROUTING(ProxorGui::dataStore->active_routing, ProxorGui::dataStore->routing.get())
                }
                w->accept();
            }
        }
    });
    connect(cancel, &QPushButton::clicked, w, &QDialog::accept);
    connect(list, &QListWidget::itemDoubleClicked, this, [=](QListWidgetItem *item) {
        lineEdit->setText(item->text());
        emit load->clicked();
    });
    w->exec();
    w->deleteLater();
}

// Direct site rules

void DialogManageRoutes::loadDirectSiteRules() {
    directSiteRules.clear();
    const auto doc = QJsonDocument::fromJson(ProxorGui::dataStore->direct_site_rules.toUtf8());
    if (!doc.isArray()) return;
    for (const auto &v: doc.array()) {
        const auto obj = v.toObject();
        DirectSiteRule rule;
        for (const auto &g: obj.value("groups").toArray()) rule.groups += g.toInt();
        for (const auto &p: obj.value("profiles").toArray()) rule.profiles += p.toInt();
        for (const auto &s: obj.value("sites").toArray()) {
            const auto site = s.toString().trimmed();
            if (!site.isEmpty()) rule.sites += site;
        }
        directSiteRules += rule;
    }
}

QString DialogManageRoutes::serializedDirectSiteRules() const {
    QJsonArray arr;
    for (const auto &rule: directSiteRules) {
        if (rule.groups.isEmpty() && rule.profiles.isEmpty() && rule.sites.isEmpty()) continue;
        QJsonArray groups, profiles, sites;
        for (int id: rule.groups) groups += id;
        for (int id: rule.profiles) profiles += id;
        for (const auto &s: rule.sites) sites += s;
        arr += QJsonObject{{"sites", sites}, {"groups", groups}, {"profiles", profiles}};
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString DialogManageRoutes::directSiteRuleSummary(const DirectSiteRule &rule) const {
    QStringList names;
    for (int gid: rule.groups) {
        const auto group = ProxorGui::profileManager->GetGroup(gid);
        names += group != nullptr ? group->name : tr("Group #%1").arg(gid);
    }
    for (int pid: rule.profiles) {
        const auto profile = ProxorGui::profileManager->GetProfile(pid);
        names += profile != nullptr ? profile->summary_name : tr("Profile #%1").arg(pid);
    }
    const QString targets = names.isEmpty() ? tr("(no targets)") : names.join(", ");
    return tr("%1  —  %n site(s)", nullptr, rule.sites.size()).arg(targets);
}

void DialogManageRoutes::commitDirectSitesEditor() {
    if (directSitesCurrent < 0 || directSitesCurrent >= directSiteRules.size()) return;
    directSiteRules[directSitesCurrent].sites = SplitLinesSkipSharp(directSitesEditor->toPlainText());
}

void DialogManageRoutes::refreshDirectSitesList() {
    const QSignalBlocker blocker(directSitesList);
    directSitesList->clear();
    for (const auto &rule: directSiteRules) {
        directSitesList->addItem(directSiteRuleSummary(rule));
    }
    if (directSitesCurrent >= 0 && directSitesCurrent < directSiteRules.size()) {
        directSitesList->setCurrentRow(directSitesCurrent);
    }
}

void DialogManageRoutes::refreshDirectSitesDetail() {
    const bool hasRule = directSitesCurrent >= 0 && directSitesCurrent < directSiteRules.size();
    directSitesTargetsBtn->setEnabled(hasRule);
    directSitesEditor->setEnabled(hasRule);
    if (!hasRule) {
        directSitesTargetsBtn->setText(tr("Targets…"));
        const QSignalBlocker blocker(directSitesEditor);
        directSitesEditor->clear();
        return;
    }
    const auto &rule = directSiteRules[directSitesCurrent];
    directSitesTargetsBtn->setText(directSiteRuleSummary(rule));
    const QSignalBlocker blocker(directSitesEditor);
    directSitesEditor->setPlainText(rule.sites.join("\n"));
}

void DialogManageRoutes::buildDirectSitesTab() {
    loadDirectSiteRules();

    auto *layout = ui->directSitesLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *hint = new QLabel(tr("Apply these sites as direct/bypass for the selected subscriptions or profiles. One site per line (same syntax as routing: example.com, domain:..., full:..., keyword:..., regexp:..., geosite:...)."), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *row = new QHBoxLayout();
    row->setSpacing(8);
    layout->addLayout(row, 1);

    auto *leftCol = new QVBoxLayout();
    leftCol->setSpacing(4);
    directSitesList = new QListWidget(this);
    directSitesList->setMinimumWidth(180);
    leftCol->addWidget(directSitesList, 1);
    auto *listBtns = new QHBoxLayout();
    auto *addRuleBtn = new QPushButton(tr("Add"), this);
    auto *removeRuleBtn = new QPushButton(tr("Remove"), this);
    listBtns->addWidget(addRuleBtn);
    listBtns->addWidget(removeRuleBtn);
    leftCol->addLayout(listBtns);
    row->addLayout(leftCol);

    auto *rightCol = new QVBoxLayout();
    rightCol->setSpacing(4);
    directSitesTargetsBtn = new QPushButton(tr("Targets…"), this);
    rightCol->addWidget(directSitesTargetsBtn);
    directSitesEditor = new QPlainTextEdit(this);
    directSitesEditor->setMinimumHeight(120);
    directSitesEditor->setPlaceholderText(tr("One direct site per line"));
    directSitesEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    rightCol->addWidget(directSitesEditor, 1);
    row->addLayout(rightCol, 1);

    connect(directSitesList, &QListWidget::currentRowChanged, this, [this](int currentRow) {
        commitDirectSitesEditor();
        directSitesCurrent = currentRow;
        refreshDirectSitesDetail();
    });
    connect(addRuleBtn, &QPushButton::clicked, this, [this] {
        commitDirectSitesEditor();
        directSiteRules += DirectSiteRule{};
        directSitesCurrent = directSiteRules.size() - 1;
        refreshDirectSitesList();
        refreshDirectSitesDetail();
    });
    connect(removeRuleBtn, &QPushButton::clicked, this, [this] {
        if (directSitesCurrent < 0 || directSitesCurrent >= directSiteRules.size()) return;
        directSiteRules.removeAt(directSitesCurrent);
        if (directSitesCurrent >= directSiteRules.size()) directSitesCurrent = directSiteRules.size() - 1;
        refreshDirectSitesList();
        refreshDirectSitesDetail();
    });
    connect(directSitesEditor, &QPlainTextEdit::textChanged, this, [this] {
        if (directSitesCurrent < 0 || directSitesCurrent >= directSiteRules.size()) return;
        directSiteRules[directSitesCurrent].sites = SplitLinesSkipSharp(directSitesEditor->toPlainText());
        const QSignalBlocker blocker(directSitesList);
        if (auto *item = directSitesList->item(directSitesCurrent)) {
            item->setText(directSiteRuleSummary(directSiteRules[directSitesCurrent]));
        }
    });
    connect(directSitesTargetsBtn, &QPushButton::clicked, this, [this] {
        if (directSitesCurrent < 0 || directSitesCurrent >= directSiteRules.size()) return;
        auto &rule = directSiteRules[directSitesCurrent];
        TargetPickerDialog dialog(rule.groups, rule.profiles, this);
        if (dialog.exec() != QDialog::Accepted) return;
        rule.groups = dialog.selectedGroups();
        rule.profiles = dialog.selectedProfiles();
        refreshDirectSitesDetail();
        const QSignalBlocker blocker(directSitesList);
        if (auto *item = directSitesList->item(directSitesCurrent)) {
            item->setText(directSiteRuleSummary(rule));
        }
    });

    directSitesCurrent = directSiteRules.isEmpty() ? -1 : 0;
    refreshDirectSitesList();
    refreshDirectSitesDetail();
}
