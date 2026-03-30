#include "./ui_mainwindow.h"
#include "mainwindow.h"

#include "fmt/Preset.hpp"
#include "db/ProfileFilter.hpp"
#include "db/ConfigBuilder.hpp"
#include "sub/GroupUpdater.hpp"
#include "sys/ExternalProcess.hpp"
#include "sys/AutoRun.hpp"

#include "ui/ThemeManager.hpp"
#include "ui/Icon.hpp"
#include "ui/edit/dialog_edit_profile.h"
#include "ui/edit/dialog_edit_group.h"
#include "ui/dialog_basic_settings.h"
#include "ui/dialog_manage_groups.h"
#include "ui/dialog_manage_routes.h"
#include "ui/dialog_vpn_settings.h"
#include "ui/dialog_ssid_settings.h"
#include "ui/dialog_hotkey.h"

#include "3rdparty/fix_old_qt.h"
#include "3rdparty/qrcodegen.hpp"
#include "3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.hpp"

#ifndef NKR_NO_ZXING
#include "3rdparty/ZxingQtReader.hpp"
#endif

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#else
#ifdef Q_OS_LINUX
#include "sys/linux/LinuxCap.h"
#endif
#include <unistd.h>
#endif

#include <QClipboard>
#include <QLabel>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QScrollBar>
#include <QScreen>
#include <QDesktopServices>
#include <QInputDialog>
#include <QThread>
#include <QTimer>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QStyleHints>

namespace {
class CenteredCheckBoxDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        QStyleOptionViewItem viewOption(option);
        initStyleOption(&viewOption, index);
        viewOption.text.clear();

        auto *style = viewOption.widget ? viewOption.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &viewOption, painter, viewOption.widget);

        if (!(index.flags() & Qt::ItemIsUserCheckable)) return;

        auto state = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        if (state == Qt::Unchecked && !index.data(Qt::CheckStateRole).isValid()) return;

        QStyleOptionButton checkbox;
        checkbox.state |= QStyle::State_Enabled;
        if (option.state & QStyle::State_MouseOver) checkbox.state |= QStyle::State_MouseOver;
        if (option.state & QStyle::State_HasFocus) checkbox.state |= QStyle::State_HasFocus;
        if (state == Qt::Checked) {
            checkbox.state |= QStyle::State_On;
        } else if (state == Qt::PartiallyChecked) {
            checkbox.state |= QStyle::State_NoChange;
        } else {
            checkbox.state |= QStyle::State_Off;
        }

        auto indicatorSize = style->sizeFromContents(QStyle::CT_CheckBox, &checkbox, QSize(), viewOption.widget);
        checkbox.rect = QStyle::alignedRect(
            option.direction,
            Qt::AlignCenter,
            indicatorSize,
            option.rect
        );
        style->drawControl(QStyle::CE_CheckBox, &checkbox, painter, viewOption.widget);
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override {
        if (!(index.flags() & Qt::ItemIsUserCheckable)) return false;

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            QStyleOptionButton checkbox;
            auto *style = option.widget ? option.widget->style() : QApplication::style();
            auto indicatorSize = style->sizeFromContents(QStyle::CT_CheckBox, &checkbox, QSize(), option.widget);
            auto indicatorRect = QStyle::alignedRect(option.direction, Qt::AlignCenter, indicatorSize, option.rect);
            if (!indicatorRect.contains(mouseEvent->pos())) return false;
        } else if (event->type() != QEvent::KeyPress) {
            return false;
        }

        auto currentState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        return model->setData(index, currentState == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    }
};

QIcon makeToggleProxyIcon(const QColor &color) {
    QPixmap pixmap(24, 24);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(color.darker(140), 1.5));
    painter.setBrush(color);
    painter.drawEllipse(QRectF(3, 3, 18, 18));

    return QIcon(pixmap);
}

QWidget *makeCenteredCheckboxCell(QWidget *parent, bool checked, const std::function<void(bool)> &onToggled) {
    auto *container = new QWidget(parent);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(6, 0, 6, 0);
    layout->setSpacing(0);
    layout->setAlignment(Qt::AlignCenter);

    auto *checkbox = new QCheckBox(container);
    checkbox->setText({});
    checkbox->setChecked(checked);
    layout->addWidget(checkbox, 0, Qt::AlignCenter);

    QObject::connect(checkbox, &QCheckBox::toggled, container, [onToggled](bool enabled) {
        onToggled(enabled);
    });
    return container;
}
}

void UI_InitMainWindow() {
    mainwindow = new MainWindow;
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    mainwindow = this;
    MW_dialog_message = [=](const QString &a, const QString &b) {
        runOnUiThread([=] { dialog_message_impl(a, b); });
    };

    // Load Manager
    ProxorGui::profileManager->LoadManager();

    // Setup misc UI
    bool legacyNumericTheme = false;
    ProxorGui::dataStore->theme.toInt(&legacyNumericTheme);
    if (legacyNumericTheme || ProxorGui::dataStore->theme.trimmed().isEmpty()) {
        ProxorGui::dataStore->theme = "System";
        ProxorGui::dataStore->Save();
    }
    themeManager->ApplyTheme(ProxorGui::dataStore->theme);
    ui->setupUi(this);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](const Qt::ColorScheme &) {
        themeManager->ApplyTheme(ProxorGui::dataStore->theme, true);
    });
#endif
    //
    connect(ui->menu_start, &QAction::triggered, this, [=]() { proxor_start(); });
    connect(ui->menu_stop, &QAction::triggered, this, [=]() { proxor_stop(); });
    connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, [=](int from, int to) {
        // use tabData to track tab & gid
        ProxorGui::profileManager->groupsTabOrder.clear();
        for (int i = 0; i < ui->tabWidget->tabBar()->count(); i++) {
            ProxorGui::profileManager->groupsTabOrder += ui->tabWidget->tabBar()->tabData(i).toInt();
        }
        ProxorGui::profileManager->SaveManager();
    });
    ui->label_running->installEventFilter(this);
    ui->label_inbound->installEventFilter(this);
    ui->splitter->installEventFilter(this);
    //
    RegisterHotkey(false);
    //
    auto last_size = ProxorGui::dataStore->mw_size.split("x");
    if (last_size.length() == 2) {
        auto w = last_size[0].toInt();
        auto h = last_size[1].toInt();
        if (w > 0 && h > 0) {
            resize(w, h);
        }
    }

    if (QDir("dashboard").count() == 0) {
        QDir().mkdir("dashboard");
        QFile::copy(":/proxor/dashboard-notice.html", "dashboard/index.html");
    }

    // top bar
    ui->toolButton_program->setMenu(ui->menu_program);
    ui->toolButton_preferences->setMenu(ui->menu_preferences);
    ui->toolButton_server->setMenu(ui->menu_server);
    const QString dropdownButtonStyle =
        "QToolButton#toolButton_program::menu-indicator,"
        "QToolButton#toolButton_preferences::menu-indicator,"
        "QToolButton#toolButton_server::menu-indicator {"
        "image: none;"
        "width: 0px;"
        "height: 0px;"
        "subcontrol-position: center;"
        "}";
    ui->toolButton_program->setStyleSheet(dropdownButtonStyle);
    ui->toolButton_preferences->setStyleSheet(dropdownButtonStyle);
    ui->toolButton_server->setStyleSheet(dropdownButtonStyle);
    ui->menubar->setVisible(false);
    ui->toolButton_toggle_proxy->setText(tr("Start"));
    ui->toolButton_toggle_proxy->setIcon(makeToggleProxyIcon(QColor(52, 199, 89)));
    ui->toolButton_toggle_proxy->setIconSize(QSize(20, 20));
    ui->toolButton_toggle_proxy->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    ui->toolButton_toggle_proxy->setStyleSheet("QToolButton#toolButton_toggle_proxy { padding: 4px; }");
    setTimeout([this] {
        const int referenceHeight = qMax(ui->toolButton_program->height(), ui->toolButton_program->sizeHint().height());
        ui->toolButton_toggle_proxy->setFixedSize(referenceHeight, referenceHeight);
    }, this, 0);
    connect(ui->toolButton_update, &QToolButton::clicked, this, [=] { runOnNewThread([=] { CheckUpdate(); }); });
    connect(ui->toolButton_url_test, &QToolButton::clicked, this, [=] { speedtest_current_group(1, true); });
    connect(ui->toolButton_update_subscription, &QToolButton::clicked, this, [=] { on_menu_update_subscription_triggered(); });
    ui->toolButton_url_test->setMinimumWidth(ui->toolButton_update_subscription->sizeHint().width());

    // Setup log UI
    ui->splitter->restoreState(DecodeB64IfValid(ProxorGui::dataStore->splitter_state));
    qvLogDocument->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setDocument(qvLogDocument);
    ui->masterLogBrowser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    {
        auto font = ui->masterLogBrowser->font();
        font.setPointSize(9);
        ui->masterLogBrowser->setFont(font);
        qvLogDocument->setDefaultFont(font);
    }
    // Log color palette follows system theme automatically.
    ui->log_filter->setFont(ui->masterLogBrowser->font());
    connect(ui->log_filter, &QLineEdit::textChanged, this, [=](const QString &text) {
        rebuildLogDocument(text);
        if (qvLogAutoScoll) {
            auto bar = ui->masterLogBrowser->verticalScrollBar();
            bar->setValue(bar->maximum());
        }
    });
    connect(ui->masterLogBrowser->verticalScrollBar(), &QSlider::valueChanged, this, [=](int value) {
        if (ui->masterLogBrowser->verticalScrollBar()->maximum() == value)
            qvLogAutoScoll = true;
        else
            qvLogAutoScoll = false;
    });
    connect(ui->masterLogBrowser, &QTextBrowser::textChanged, this, [=]() {
        if (!qvLogAutoScoll)
            return;
        auto bar = ui->masterLogBrowser->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    MW_show_log = [=](const QString &log) {
        runOnUiThread([=] { show_log_impl(log); });
    };
    MW_show_log_ext = [=](const QString &tag, const QString &log) {
        runOnUiThread([=] { show_log_impl("[" + tag + "] " + log); });
    };
    MW_show_log_ext_vt100 = [=](const QString &log) {
        runOnUiThread([=] { show_log_impl(log); });
    };

    // table UI
    ui->proxyListTable->callback_save_order = [=] {
        auto group = ProxorGui::profileManager->CurrentGroup();
        group->order = ui->proxyListTable->order;
        ProxorGui::profileManager->SaveGroup(group);
    };
    ui->proxyListTable->refresh_data = [=](int id) { refresh_proxy_list_impl_refresh_data(id); };
    if (auto button = ui->proxyListTable->findChild<QAbstractButton *>(QString(), Qt::FindDirectChildrenOnly)) {
        // Corner Button
        connect(button, &QAbstractButton::clicked, this, [=] { refresh_proxy_list_impl(-1, {GroupSortMethod::ById}); });
    }
    connect(ui->proxyListTable->horizontalHeader(), &QHeaderView::sectionClicked, this, [=](int logicalIndex) {
        GroupSortAction action;
        // 不正确的descending实现
        if (proxy_last_order == logicalIndex) {
            action.descending = true;
            proxy_last_order = -1;
        } else {
            proxy_last_order = logicalIndex;
        }
        action.save_sort = true;
        // 表头
        if (logicalIndex == 0) {
            return;
        } else if (logicalIndex == 1) {
            action.method = GroupSortMethod::ByName;
        } else if (logicalIndex == 2) {
            action.method = GroupSortMethod::ByType;
        } else if (logicalIndex == 3) {
            action.method = GroupSortMethod::ByAddress;
        } else if (logicalIndex == 4) {
            action.method = GroupSortMethod::ByLatency;
        } else {
            return;
        }
        refresh_proxy_list_impl(-1, action);
    });
    connect(ui->proxyListTable->horizontalHeader(), &QHeaderView::sectionResized, this, [=](int logicalIndex, int oldSize, int newSize) {
        auto group = ProxorGui::profileManager->CurrentGroup();
        if (ProxorGui::dataStore->refreshing_group || group == nullptr || !group->manually_column_width) return;
        // per-column minimum widths: name=-25%, address=-25%, traffic=+20% (baseline 100px)
        static const int colMinWidths[] = {0, 75, 0, 75, 0, 120};
        int minW = (logicalIndex < 6) ? colMinWidths[logicalIndex] : 0;
        if (minW > 0 && newSize < minW) {
            auto header = ui->proxyListTable->horizontalHeader();
            header->blockSignals(true);
            header->resizeSection(logicalIndex, minW);
            header->blockSignals(false);
            newSize = minW;
        }
        // save manually column width
        group->column_width.clear();
        for (int i = 0; i < ui->proxyListTable->horizontalHeader()->count(); i++) {
            group->column_width.push_back(ui->proxyListTable->horizontalHeader()->sectionSize(i));
        }
        group->column_width[logicalIndex] = newSize;
        ProxorGui::profileManager->SaveGroup(group);
    });
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->setSortingEnabled(true);
    ui->tableWidget_conn->horizontalHeader()->setSectionsClickable(true);
    ui->tableWidget_conn->horizontalHeader()->setSortIndicatorShown(true);
    // Default sort: descending by Status col so active connections appear first
    ui->tableWidget_conn->sortByColumn(0, Qt::DescendingOrder);
    // Connection tab filter
    auto applyConnFilter = [=](const QString &text) {
        const bool hasFilter = !text.isEmpty();
        const int rows = ui->tableWidget_conn->rowCount();
        for (int r = 0; r < rows; ++r) {
            if (!hasFilter) {
                ui->tableWidget_conn->setRowHidden(r, false);
                continue;
            }
            bool match = false;
            // Search columns 1 (Outbound), 2 (Destination), 3 (Process), 4 (Protocol)
            for (int c = 1; c <= 4; ++c) {
                auto *item = ui->tableWidget_conn->item(r, c);
                if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
            ui->tableWidget_conn->setRowHidden(r, !match);
        }
    };
    connect(ui->conn_filter, &QLineEdit::textChanged, this, applyConnFilter);
    ui->proxyListTable->verticalHeader()->setDefaultSectionSize(24);

    // search box
    ui->search->setVisible(false);
    connect(shortcut_ctrl_f, &QShortcut::activated, this, [=] {
        ui->search->setVisible(true);
        ui->search->setFocus();
    });
    connect(shortcut_esc, &QShortcut::activated, this, [=] {
        if (ui->search->isVisible()) {
            ui->search->setText("");
            ui->search->textChanged("");
            ui->search->setVisible(false);
        }
        if (select_mode) {
            emit profile_selected(-1);
            select_mode = false;
            refresh_status();
        }
    });
    connect(ui->search, &QLineEdit::textChanged, this, [=](const QString &text) {
        if (text.isEmpty()) {
            for (int i = 0; i < ui->proxyListTable->rowCount(); i++) {
                ui->proxyListTable->setRowHidden(i, false);
            }
        } else {
            QList<QTableWidgetItem *> findItem = ui->proxyListTable->findItems(text, Qt::MatchContains);
            for (int i = 0; i < ui->proxyListTable->rowCount(); i++) {
                ui->proxyListTable->setRowHidden(i, true);
            }
            for (auto item: findItem) {
                if (item != nullptr) ui->proxyListTable->setRowHidden(item->row(), false);
            }
        }
    });

    // refresh
    this->refresh_groups();

    // Setup Tray
    tray = new QSystemTrayIcon(this); // 初始化托盘对象tray
    tray->setIcon(Icon::GetTrayIcon(Icon::NONE));
    tray->setContextMenu(ui->menu_program); // 创建托盘菜单
    tray->show();                           // 让托盘图标显示在系统托盘上
    connect(tray, &QSystemTrayIcon::activated, this, [=](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (this->isVisible()) {
                hide();
            } else {
                ActivateWindow(this);
            }
        }
    });

    // Misc menu
    connect(ui->menu_open_config_folder, &QAction::triggered, this, [=] { QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath())); });
    ui->menu_program_preference->addActions(ui->menu_preferences->actions());
    connect(ui->actionRestart_Proxy, &QAction::triggered, this, [=] { if (ProxorGui::dataStore->started_id>=0) proxor_start(ProxorGui::dataStore->started_id); });
    connect(ui->actionRestart_Program, &QAction::triggered, this, [=] { MW_dialog_message("", "RestartProgram"); });
    connect(ui->actionShow_window, &QAction::triggered, this, [=] { tray->activated(QSystemTrayIcon::ActivationReason::Trigger); });
    //
    connect(ui->menu_program, &QMenu::aboutToShow, this, [=]() {
        ui->actionRemember_last_proxy->setChecked(ProxorGui::dataStore->remember_enable);
        ui->actionStart_with_system->setChecked(AutoRun_IsEnabled());
        ui->actionAllow_LAN->setChecked(QStringList{"::", "0.0.0.0"}.contains(ProxorGui::dataStore->inbound_address));
        // active server
        for (const auto &old: ui->menuActive_Server->actions()) {
            ui->menuActive_Server->removeAction(old);
            old->deleteLater();
        }
        int active_server_item_count = 0;
        for (const auto &pf: ProxorGui::profileManager->CurrentGroup()->ProfilesWithOrder()) {
            auto a = new QAction(pf->bean->DisplayTypeAndName(), this);
            a->setProperty("id", pf->id);
            a->setCheckable(true);
            if (ProxorGui::dataStore->started_id == pf->id) a->setChecked(true);
            ui->menuActive_Server->addAction(a);
            if (++active_server_item_count == 100) break;
        }
        // active routing
        for (const auto &old: ui->menuActive_Routing->actions()) {
            ui->menuActive_Routing->removeAction(old);
            old->deleteLater();
        }
        for (const auto &name: ProxorGui::Routing::List()) {
            auto a = new QAction(name, this);
            a->setCheckable(true);
            a->setChecked(name == ProxorGui::dataStore->active_routing);
            ui->menuActive_Routing->addAction(a);
        }
    });
    connect(ui->menuActive_Server, &QMenu::triggered, this, [=](QAction *a) {
        bool ok;
        auto id = a->property("id").toInt(&ok);
        if (!ok) return;
        if (ProxorGui::dataStore->started_id == id) {
            proxor_stop();
        } else {
            proxor_start(id);
        }
    });
    connect(ui->menuActive_Routing, &QMenu::triggered, this, [=](QAction *a) {
        auto fn = a->text();
        if (!fn.isEmpty()) {
            ProxorGui::Routing r;
            r.load_control_must = true;
            r.fn = ROUTES_PREFIX + fn;
            if (r.Load()) {
                if (QMessageBox::question(GetMessageBoxParent(), software_name, tr("Load routing and apply: %1").arg(fn) + "\n" + r.DisplayRouting()) == QMessageBox::Yes) {
                    ProxorGui::Routing::SetToActive(fn);
                    if (ProxorGui::dataStore->started_id >= 0) {
                        proxor_start(ProxorGui::dataStore->started_id);
                    } else {
                        refresh_status();
                    }
                }
            }
        }
    });
    connect(ui->actionRemember_last_proxy, &QAction::triggered, this, [=](bool checked) {
        ProxorGui::dataStore->remember_enable = checked;
        ProxorGui::dataStore->Save();
    });
    connect(ui->actionStart_with_system, &QAction::triggered, this, [=](bool checked) {
        AutoRun_SetEnabled(checked);
    });
    connect(ui->actionAllow_LAN, &QAction::triggered, this, [=](bool checked) {
        ProxorGui::dataStore->inbound_address = checked ? "::" : "127.0.0.1";
        MW_dialog_message("", "UpdateDataStore");
    });
    //
    connect(ui->checkBox_VPN, &QCheckBox::clicked, this, [=](bool checked) { proxor_set_spmode_vpn(checked); });
    connect(ui->checkBox_SystemProxy, &QCheckBox::clicked, this, [=](bool checked) { proxor_set_spmode_system_proxy(checked); });
    connect(ui->menu_spmode, &QMenu::aboutToShow, this, [=]() {
        ui->menu_spmode_disabled->setChecked(!(ProxorGui::dataStore->spmode_system_proxy || ProxorGui::dataStore->spmode_vpn));
        ui->menu_spmode_system_proxy->setChecked(ProxorGui::dataStore->spmode_system_proxy);
        ui->menu_spmode_vpn->setChecked(ProxorGui::dataStore->spmode_vpn);
    });
    connect(ui->menu_spmode_system_proxy, &QAction::triggered, this, [=](bool checked) { proxor_set_spmode_system_proxy(checked); });
    connect(ui->menu_spmode_vpn, &QAction::triggered, this, [=](bool checked) { proxor_set_spmode_vpn(checked); });
    connect(ui->menu_spmode_disabled, &QAction::triggered, this, [=]() {
        proxor_set_spmode_system_proxy(false);
        proxor_set_spmode_vpn(false);
    });
    connect(ui->menu_qr, &QAction::triggered, this, [=]() { display_qr_link(false); });
    connect(ui->menu_tcp_ping, &QAction::triggered, this, [=]() { speedtest_current_group(0, false); });
    connect(ui->menu_url_test, &QAction::triggered, this, [=]() { speedtest_current_group(1, false); });
    connect(ui->menu_full_test, &QAction::triggered, this, [=]() { speedtest_current_group(2, false); });
    connect(ui->menu_stop_testing, &QAction::triggered, this, [=]() { speedtest_current_group(114514, false); });
    //
    auto set_selected_or_group = [=](int mode) {
        // 0=group 1=select 2=unknown(menu is hide)
        ui->menu_server->setProperty("selected_or_group", mode);
    };
    auto move_tests_to_menu = [=](bool menuCurrent_Select) {
        return [=] {
            if (menuCurrent_Select) {
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_tcp_ping);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_url_test);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_full_test);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_stop_testing);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_clear_test_result);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_resolve_domain);
            } else {
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_tcp_ping);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_url_test);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_full_test);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_stop_testing);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_clear_test_result);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_resolve_domain);
            }
            set_selected_or_group(menuCurrent_Select ? 1 : 0);
        };
    };
    connect(ui->menuCurrent_Select, &QMenu::aboutToShow, this, move_tests_to_menu(true));
    connect(ui->menuCurrent_Group, &QMenu::aboutToShow, this, move_tests_to_menu(false));
    connect(ui->menu_server, &QMenu::aboutToHide, this, [=] {
        setTimeout([=] { set_selected_or_group(2); }, this, 200);
    });
    set_selected_or_group(2);
    //
    connect(ui->menu_share_item, &QMenu::aboutToShow, this, [=] {
        QString name;
        auto selected = get_now_selected_list();
        if (!selected.isEmpty()) {
            auto ent = selected.first();
            name = ent->bean->DisplayCoreType();
        }
        ui->menu_export_config->setVisible(name == software_core_name);
        ui->menu_export_config->setText(tr("Export %1 config").arg(name));
    });
    refresh_status();

    // Prepare core
    ProxorGui::dataStore->core_token = GetRandomString(32);
    ProxorGui::dataStore->core_port = MkPort();
    if (ProxorGui::dataStore->core_port <= 0) ProxorGui::dataStore->core_port = 19810;

    auto core_path = ProxorGui::FindProxorCoreRealPath();

    QStringList args;
    args.push_back("proxor");
    args.push_back("-port");
    args.push_back(Int2String(ProxorGui::dataStore->core_port));
    if (!ProxorGui::dataStore->core_enable_color) args.push_back("-disable-color");
    if (ProxorGui::dataStore->flag_debug) args.push_back("-debug");

    // Start core
    runOnUiThread(
        [=] {
            core_process = new ProxorGui_sys::CoreProcess(core_path, args);
            // Remember last started
            if (ProxorGui::dataStore->remember_enable && ProxorGui::dataStore->remember_id >= 0) {
                core_process->start_profile_when_core_is_up = ProxorGui::dataStore->remember_id;
            }
            // Setup
            core_process->Start();
            setup_grpc();
        },
        DS_cores);

    wifi_monitor = new WifiMonitor(this);
    connect(wifi_monitor, &WifiMonitor::ssidChanged, this, &MainWindow::onWifiSsidChanged);
    wifi_monitor->start();

    const bool restore_system_proxy = ProxorGui::dataStore->remember_spmode.contains("system_proxy");
    const bool restore_vpn = ProxorGui::dataStore->remember_spmode.contains("vpn") || ProxorGui::dataStore->flag_restart_tun_on;

    connect(qApp, &QGuiApplication::commitDataRequest, this, &MainWindow::on_commitDataRequest);

    auto t = new QTimer;
    connect(t, &QTimer::timeout, this, [=]() { refresh_status(); });
    t->start(2000);

    t = new QTimer;
    connect(t, &QTimer::timeout, this, [&] { ProxorGui_sys::logCounter.fetchAndStoreRelaxed(0); });
    t->start(1000);

    TM_auto_update_subsctiption = new QTimer;
    TM_auto_update_subsctiption_Reset_Minute = [&](int m) {
        TM_auto_update_subsctiption->stop();
        if (m >= 30) TM_auto_update_subsctiption->start(m * 60 * 1000);
    };
    connect(TM_auto_update_subsctiption, &QTimer::timeout, this, [&] { UI_update_all_groups(true); });
    TM_auto_update_subsctiption_Reset_Minute(ProxorGui::dataStore->sub_auto_update);

    if (ProxorGui::dataStore->check_update_on_start) {
        setTimeout([this] {
            runOnNewThread([this] { CheckUpdate(true); });
        }, this, 1500);
    }

    if (!ProxorGui::dataStore->flag_tray) show();

    // Restore spmode after the window has entered the event loop so prompts
    // like the Tun admin warning are shown the same way as manual activation.
    if (ProxorGui::dataStore->remember_enable || ProxorGui::dataStore->flag_restart_tun_on || restore_vpn) {
        setTimeout([=] {
            if (restore_system_proxy) {
                proxor_set_spmode_system_proxy(true, false);
            }
            if (restore_vpn) {
                proxor_set_spmode_vpn(true, false);
            }
        }, this, 0);
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (tray->isVisible()) {
        hide();          // 隐藏窗口
        event->ignore(); // 忽略事件
    }
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::onWifiSsidChanged(const QString &ssid) {
    if (!ProxorGui::dataStore->ssid_on_demand_enabled) return;

    bool isTrigger = !ssid.isEmpty() &&
                     ProxorGui::dataStore->ssid_trigger_list.contains(ssid, Qt::CaseSensitive);

    if (isTrigger) {
        // Start with the last-used profile if one exists and the proxy isn't already running
        auto targetId = ProxorGui::dataStore->remember_id;
        if (targetId >= 0 && ProxorGui::dataStore->started_id < 0) {
            MW_show_log(tr("[On-Demand] Trigger SSID \"%1\" detected — starting profile %2").arg(ssid).arg(targetId));
            proxor_start(targetId);
        }
    } else {
        // Non-trigger SSID or disconnected — stop if the proxy is running
        if (ProxorGui::dataStore->started_id >= 0) {
            MW_show_log(tr("[On-Demand] Non-trigger SSID \"%1\" — stopping proxy").arg(ssid));
            proxor_stop(false, false);
        }
    }
}

// Group tab manage

inline int tabIndex2GroupId(int index) {
    if (ProxorGui::profileManager->groupsTabOrder.length() <= index) return -1;
    return ProxorGui::profileManager->groupsTabOrder[index];
}

inline int groupId2TabIndex(int gid) {
    for (int key = 0; key < ProxorGui::profileManager->groupsTabOrder.count(); key++) {
        if (ProxorGui::profileManager->groupsTabOrder[key] == gid) return key;
    }
    return 0;
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if (ProxorGui::dataStore->refreshing_group_list) return;
    if (tabIndex2GroupId(index) == ProxorGui::dataStore->current_group) return;
    show_group(tabIndex2GroupId(index));
}

void MainWindow::show_group(int gid) {
    if (ProxorGui::dataStore->refreshing_group) return;
    ProxorGui::dataStore->refreshing_group = true;
    QCheckBox toggleCheckboxPrototype;
    const int toggleColumnWidth = toggleCheckboxPrototype.sizeHint().width() + 16;

    auto group = ProxorGui::profileManager->GetGroup(gid);
    if (group == nullptr) {
        MessageBoxWarning(tr("Error"), QStringLiteral("No such group: %1").arg(gid));
        ProxorGui::dataStore->refreshing_group = false;
        return;
    }

    if (ProxorGui::dataStore->current_group != gid) {
        ProxorGui::dataStore->current_group = gid;
        ProxorGui::dataStore->Save();
    }
    ui->tabWidget->widget(groupId2TabIndex(gid))->layout()->addWidget(ui->proxyListTable);

    // 列宽是否可调
    if (group->manually_column_width) {
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        ui->proxyListTable->horizontalHeader()->resizeSection(0, toggleColumnWidth);
        static const int defaultColWidths[] = {0, 75, 0, 75, 0, 120};
        for (int i = 1; i <= 5; i++) {
            ui->proxyListTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
            auto size = group->column_width.value(i);
            if (size <= 0) {
                int defW = (i < 6) ? defaultColWidths[i] : 0;
                size = (defW > 0) ? defW : ui->proxyListTable->horizontalHeader()->defaultSectionSize();
            }
            ui->proxyListTable->horizontalHeader()->resizeSection(i, size);
        }
    } else {
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        ui->proxyListTable->horizontalHeader()->resizeSection(0, toggleColumnWidth);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    }

    // Quota column: only visible for subscription groups
    bool isSub = !group->url.isEmpty();
    ui->proxyListTable->setColumnHidden(6, !isSub);
    if (isSub) {
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    }

    // show proxies
    GroupSortAction gsa;
    gsa.scroll_to_started = true;
    refresh_proxy_list_impl(-1, gsa);

    ProxorGui::dataStore->refreshing_group = false;
}

// callback

void MainWindow::dialog_message_impl(const QString &sender, const QString &info) {
    // info
    if (info.contains("UpdateIcon")) {
        icon_status = -1;
        refresh_status();
    }
    if (info.contains("UpdateDataStore")) {
        auto suggestRestartProxy = ProxorGui::dataStore->Save();
        if (info.contains("RouteChanged")) {
            suggestRestartProxy = true;
        }
        if (info.contains("NeedRestart")) {
            suggestRestartProxy = false;
        }
        refresh_proxy_list();
        if (info.contains("VPNChanged") && ProxorGui::dataStore->spmode_vpn) {
            MessageBoxWarning(tr("Tun Settings changed"), tr("Restart Tun to take effect."));
        }
        if (suggestRestartProxy && ProxorGui::dataStore->started_id >= 0 &&
            QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
            proxor_start(ProxorGui::dataStore->started_id);
        }
        refresh_status();
    }
    if (info.contains("NeedRestart")) {
        auto n = QMessageBox::warning(GetMessageBoxParent(), tr("Settings changed"), tr("Restart the program to take effect."), QMessageBox::Yes | QMessageBox::No);
        if (n == QMessageBox::Yes) {
            this->exit_reason = 2;
            on_menu_exit_triggered();
        }
    }
    //
    if (info == "RestartProgram") {
        this->exit_reason = 2;
        on_menu_exit_triggered();
    } else if (info == "Raise") {
        ActivateWindow(this);
    } else if (info == "ClearConnectionList") {
        refresh_connection_list({});
    }
    // sender
    if (sender == Dialog_DialogEditProfile) {
        auto msg = info.split(",");
        if (msg.contains("accept")) {
            refresh_proxy_list();
            if (msg.contains("restart")) {
                if (QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
                    proxor_start(ProxorGui::dataStore->started_id);
                }
            }
        }
    } else if (sender == Dialog_DialogManageGroups) {
        if (info.startsWith("refresh")) {
            this->refresh_groups();
        }
    } else if (sender == "SubUpdater") {
        if (info.startsWith("finish")) {
            refresh_proxy_list();
            if (!info.contains("dingyue")) {
                show_log_impl(tr("Imported %1 profile(s)").arg(ProxorGui::dataStore->imported_count));
            }
        } else if (info == "NewGroup") {
            refresh_groups();
        }
    } else if (sender == "ExternalProcess") {
        if (info == "Crashed") {
            proxor_stop();
        } else if (info == "CoreCrashed") {
            proxor_stop(true);
        } else if (info.startsWith("CoreStarted")) {
            proxor_start(info.split(",")[1].toInt());
        }
    }
}

// top bar & tray menu

inline bool dialog_is_using = false;

#define USE_DIALOG(a)                               \
    if (dialog_is_using) return;                    \
    dialog_is_using = true;                         \
    auto dialog = new a(this);                      \
    connect(dialog, &QDialog::finished, this, [=] { \
        dialog->deleteLater();                      \
        dialog_is_using = false;                    \
    });                                             \
    dialog->show();

void MainWindow::on_menu_basic_settings_triggered() {
    USE_DIALOG(DialogBasicSettings)
}

void MainWindow::on_menu_manage_groups_triggered() {
    USE_DIALOG(DialogManageGroups)
}

void MainWindow::on_menu_routing_settings_triggered() {
    USE_DIALOG(DialogManageRoutes)
}

void MainWindow::on_menu_vpn_settings_triggered() {
    USE_DIALOG(DialogVPNSettings)
}

void MainWindow::on_menu_ssid_settings_triggered() {
    USE_DIALOG(DialogSSIDSettings)
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    USE_DIALOG(DialogHotkey)
}

void MainWindow::on_commitDataRequest() {
    qDebug() << "Start of data save";
    //
    if (!isMaximized()) {
        auto olds = ProxorGui::dataStore->mw_size;
        auto news = QStringLiteral("%1x%2").arg(size().width()).arg(size().height());
        if (olds != news) {
            ProxorGui::dataStore->mw_size = news;
        }
    }
    //
    ProxorGui::dataStore->splitter_state = ui->splitter->saveState().toBase64();
    //
    auto last_id = ProxorGui::dataStore->started_id;
    if (ProxorGui::dataStore->remember_enable && last_id >= 0) {
        ProxorGui::dataStore->remember_id = last_id;
    }
    //
    ProxorGui::dataStore->Save();
    ProxorGui::profileManager->SaveManager();
    qDebug() << "End of data save";
}

void MainWindow::onUpdateStaged() {
    update_staged = true;
    tray->showMessage(
        tr("Proxor"),
        tr("Update downloaded. It will install automatically when you close the app."),
        QSystemTrayIcon::Information,
        4000
    );
    // User expects immediate restart after manually clicking update
    this->exit_reason = 1;
    on_menu_exit_triggered();
}

void MainWindow::on_menu_exit_triggered() {
    if (mu_exit.tryLock()) {
        if (update_staged && exit_reason == 0) {
            exit_reason = 1;
        }
        ProxorGui::dataStore->prepare_exit = true;
        //
        proxor_set_spmode_system_proxy(false, false);
        proxor_set_spmode_vpn(false, false);
        if (ProxorGui::dataStore->spmode_vpn) {
            mu_exit.unlock(); // retry
            return;
        }
        RegisterHotkey(true);
        //
        on_commitDataRequest();
        //
        ProxorGui::dataStore->save_control_no_save = true; // don't change datastore after this line
        proxor_stop(false, true);
        //
        hide();
        runOnNewThread([=] {
            sem_stopped.acquire();
            stop_core_daemon();
            runOnUiThread([=] {
                on_menu_exit_triggered(); // continue exit progress
            });
        });
        return;
    }
    //
    MF_release_runguard();
    if (exit_reason == 1) {
        QDir::setCurrent(ProxorGui::PackageRootPath());
#ifdef Q_OS_WIN
        QProcess::startDetached(ProxorGui::PackageExecutablePath("updater"), QStringList{});
#else
        QProcess::startDetached("./updater", QStringList{});
#endif
    } else if (exit_reason == 2 || exit_reason == 3) {
        QDir::setCurrent(ProxorGui::PackageRootPath());

        auto arguments = ProxorGui::dataStore->argv;
        if (arguments.length() > 0) {
            arguments.removeFirst();
            arguments.removeAll("-tray");
            arguments.removeAll("-flag_restart_tun_on");
            arguments.removeAll("-flag_reorder");
        }
        auto isLauncher = qEnvironmentVariable("NKR_FROM_LAUNCHER") == "1";
        if (isLauncher) arguments.prepend("--");
#ifdef Q_OS_WIN
        auto program = ProxorGui::PackageExecutablePath("proxor");
#else
        auto program = isLauncher ? "./launcher" : QApplication::applicationFilePath();
#endif

        if (exit_reason == 3) {
            // Tun restart as admin
            arguments << "-flag_restart_tun_on";
#ifdef Q_OS_WIN
            WinCommander::runProcessElevated(program, arguments, "", WinCommander::SW_NORMAL, false);
#else
            QProcess::startDetached(program, arguments);
#endif
        } else {
            QProcess::startDetached(program, arguments);
        }
    }
    QApplication::closeAllWindows();
    tray->hide();
    QCoreApplication::quit();
}

#define proxor_set_spmode_FAILED \
    refresh_status();          \
    return;

void MainWindow::proxor_set_spmode_system_proxy(bool enable, bool save) {
    if (enable != ProxorGui::dataStore->spmode_system_proxy) {
        if (enable) {
            auto socks_port = ProxorGui::dataStore->inbound_socks_port;
            auto http_port = ProxorGui::dataStore->inbound_socks_port;
            SetSystemProxy(http_port, socks_port);
        } else {
            ClearSystemProxy();
        }
    }

    if (save) {
        ProxorGui::dataStore->remember_spmode.removeAll("system_proxy");
        if (enable && ProxorGui::dataStore->remember_enable) {
            ProxorGui::dataStore->remember_spmode.append("system_proxy");
        }
        ProxorGui::dataStore->Save();
    }

    ProxorGui::dataStore->spmode_system_proxy = enable;
    refresh_status();
}

void MainWindow::proxor_set_spmode_vpn(bool enable, bool save) {
    if (enable != ProxorGui::dataStore->spmode_vpn) {
        if (enable) {
            if (ProxorGui::dataStore->vpn_internal_tun) {
                bool requestPermission = !ProxorGui::IsAdmin();
                if (requestPermission) {
#ifdef Q_OS_LINUX
                    if (!Linux_HavePkexec()) {
                        MessageBoxWarning(software_name, "Please install \"pkexec\" first.");
                        proxor_set_spmode_FAILED
                    }
                    auto ret = Linux_Pkexec_SetCapString(ProxorGui::FindProxorCoreRealPath(), "cap_net_admin=ep");
                    if (ret == 0) {
                        this->exit_reason = 3;
                        on_menu_exit_triggered();
                    } else {
                        MessageBoxWarning(software_name, "Setcap for Tun mode failed.\n\n1. You may canceled the dialog.\n2. You may be using an incompatible environment like AppImage.");
                        if (QProcessEnvironment::systemEnvironment().contains("APPIMAGE")) {
                            MW_show_log("If you are using AppImage, it's impossible to start a Tun. Please use other package instead.");
                        }
                    }
#endif
#ifdef Q_OS_WIN
                    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please run Proxor as admin"), QMessageBox::Yes | QMessageBox::No);
                    if (n == QMessageBox::Yes) {
                        this->exit_reason = 3;
                        on_menu_exit_triggered();
                    }
#endif
                    proxor_set_spmode_FAILED
                }
            } else {
                if (ProxorGui::dataStore->need_keep_vpn_off) {
                    MessageBoxWarning(software_name, tr("Current server is incompatible with Tun. Please stop the server first, enable Tun Mode, and then restart."));
                    proxor_set_spmode_FAILED
                }
                if (!StartVPNProcess()) {
                    proxor_set_spmode_FAILED
                }
            }
        } else {
            if (ProxorGui::dataStore->vpn_internal_tun) {
                // current core is sing-box
            } else {
                if (!StopVPNProcess()) {
                    proxor_set_spmode_FAILED
                }
            }
        }
    }

    if (save) {
        ProxorGui::dataStore->remember_spmode.removeAll("vpn");
        if (enable) {
            ProxorGui::dataStore->remember_spmode.append("vpn");
        }
        ProxorGui::dataStore->Save();
    }

    ProxorGui::dataStore->spmode_vpn = enable;
    refresh_status();

    if (ProxorGui::dataStore->vpn_internal_tun && ProxorGui::dataStore->started_id >= 0) proxor_start(ProxorGui::dataStore->started_id);
}

void MainWindow::refresh_status(const QString &traffic_update) {
    auto refresh_speed_label = [=] {
        if (traffic_update_cache == "") {
            ui->label_speed->setText(QObject::tr("Proxy: %1\nDirect: %2").arg("", ""));
        } else {
            ui->label_speed->setText(traffic_update_cache);
        }
    };

    // From TrafficLooper
    if (!traffic_update.isEmpty()) {
        traffic_update_cache = traffic_update;
        if (traffic_update == "STOP") {
            traffic_update_cache = "";
        } else {
            refresh_speed_label();
            return;
        }
    }

    refresh_speed_label();

    // From UI
    QString group_name;
    if (running != nullptr) {
        auto group = ProxorGui::profileManager->GetGroup(running->gid);
        if (group != nullptr) group_name = group->name;
    }

    if (last_test_time.addSecs(2) < QTime::currentTime()) {
        auto txt = running == nullptr
                       ? (start_pending ? tr("Starting...") : tr("Not Running"))
                       : QStringLiteral("[%1] %2").arg(group_name, running->bean->DisplayName()).left(30);
        ui->label_running->setText(txt);
    }
    //
    auto display_socks = DisplayAddress(ProxorGui::dataStore->inbound_address, ProxorGui::dataStore->inbound_socks_port);
    auto inbound_txt = QStringLiteral("Mixed: %1").arg(display_socks);
    ui->label_inbound->setText(inbound_txt);
    //
    ui->checkBox_VPN->setChecked(ProxorGui::dataStore->spmode_vpn);
    ui->checkBox_SystemProxy->setChecked(ProxorGui::dataStore->spmode_system_proxy);
    const bool showStopState = running != nullptr || start_pending;
    ui->toolButton_toggle_proxy->setText(showStopState ? tr("Stop") : tr("Start"));
    ui->toolButton_toggle_proxy->setIcon(showStopState ? makeToggleProxyIcon(QColor(255, 59, 48))
                                                       : makeToggleProxyIcon(QColor(52, 199, 89)));
    if (select_mode) {
        ui->label_running->setText(tr("Select") + " *");
        ui->label_running->setToolTip(tr("Select mode, double-click or press Enter to select a profile, press ESC to exit."));
    } else {
        ui->label_running->setToolTip({});
    }

    auto make_title = [=](bool isTray) {
        QStringList tt;
        if (!isTray && ProxorGui::IsAdmin()) tt << "[Admin]";
        if (select_mode) tt << "[" + tr("Select") + "]";
        if (!title_error.isEmpty()) tt << "[" + title_error + "]";
        if (ProxorGui::dataStore->spmode_vpn && !ProxorGui::dataStore->spmode_system_proxy) tt << "[Tun]";
        if (!ProxorGui::dataStore->spmode_vpn && ProxorGui::dataStore->spmode_system_proxy) tt << "[" + tr("System Proxy") + "]";
        if (ProxorGui::dataStore->spmode_vpn && ProxorGui::dataStore->spmode_system_proxy) tt << "[Tun+" + tr("System Proxy") + "]";
        tt << software_name;
        if (!isTray) tt << "(" + QString(NKR_VERSION) + ")";
        if (!ProxorGui::dataStore->active_routing.isEmpty() && ProxorGui::dataStore->active_routing != "Default") {
            tt << "[" + ProxorGui::dataStore->active_routing + "]";
        }
        if (running != nullptr) tt << running->bean->DisplayTypeAndName() + "@" + group_name;
        return tt.join(isTray ? "\n" : " ");
    };

    auto icon_status_new = Icon::NONE;

    if (running != nullptr) {
        if (ProxorGui::dataStore->spmode_vpn) {
            icon_status_new = Icon::VPN;
        } else if (ProxorGui::dataStore->spmode_system_proxy) {
            icon_status_new = Icon::SYSTEM_PROXY;
        } else {
            icon_status_new = Icon::RUNNING;
        }
    }

    // refresh title & window icon
    setWindowTitle(make_title(false));
    if (icon_status_new != icon_status) QApplication::setWindowIcon(Icon::GetTrayIcon(Icon::NONE));

    // refresh tray
    if (tray != nullptr) {
        tray->setToolTip(make_title(true));
        if (icon_status_new != icon_status) tray->setIcon(Icon::GetTrayIcon(icon_status_new));
    }

    icon_status = icon_status_new;
}

// table显示

// refresh_groups -> show_group -> refresh_proxy_list
void MainWindow::refresh_groups() {
    ProxorGui::dataStore->refreshing_group_list = true;

    // refresh group?
    for (int i = ui->tabWidget->count() - 1; i > 0; i--) {
        ui->tabWidget->removeTab(i);
    }

    int index = 0;
    for (const auto &gid: ProxorGui::profileManager->groupsTabOrder) {
        auto group = ProxorGui::profileManager->GetGroup(gid);
        if (index == 0) {
            ui->tabWidget->setTabText(0, group->name);
        } else {
            auto widget2 = new QWidget();
            auto layout2 = new QVBoxLayout();
            layout2->setContentsMargins(QMargins());
            layout2->setSpacing(0);
            widget2->setLayout(layout2);
            ui->tabWidget->addTab(widget2, group->name);
        }
        ui->tabWidget->tabBar()->setTabData(index, gid);
        index++;
    }

    // show after group changed
    if (ProxorGui::profileManager->CurrentGroup() == nullptr) {
        ProxorGui::dataStore->current_group = -1;
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(0));
        show_group(ProxorGui::profileManager->groupsTabOrder.count() > 0 ? ProxorGui::profileManager->groupsTabOrder.first() : 0);
    } else {
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(ProxorGui::dataStore->current_group));
        show_group(ProxorGui::dataStore->current_group);
    }

    ProxorGui::dataStore->refreshing_group_list = false;
}

void MainWindow::refresh_proxy_list(const int &id) {
    refresh_proxy_list_impl(id, {});
}

void MainWindow::refresh_proxy_list_impl(const int &id, GroupSortAction groupSortAction) {
    // id < 0 重绘
    if (id < 0) {
        // 清空数据
        ui->proxyListTable->row2Id.clear();
        ui->proxyListTable->setRowCount(0);
        // 添加行
        int row = -1;
        for (const auto &[id, profile]: ProxorGui::profileManager->profiles) {
            if (ProxorGui::dataStore->current_group != profile->gid) continue;
            row++;
            ui->proxyListTable->insertRow(row);
            ui->proxyListTable->row2Id += id;
        }
    }

    // 显示排序
    if (id < 0) {
        switch (groupSortAction.method) {
            case GroupSortMethod::Raw: {
                auto group = ProxorGui::profileManager->CurrentGroup();
                if (group == nullptr) return;
                ui->proxyListTable->order = group->order;
                break;
            }
            case GroupSortMethod::ById: {
                // Clear Order
                ui->proxyListTable->order.clear();
                ui->proxyListTable->callback_save_order();
                break;
            }
            case GroupSortMethod::ByAddress:
            case GroupSortMethod::ByName:
            case GroupSortMethod::ByLatency:
            case GroupSortMethod::ByType: {
                std::sort(ui->proxyListTable->order.begin(), ui->proxyListTable->order.end(),
                          [=](int a, int b) {
                              QString ms_a;
                              QString ms_b;
                              if (groupSortAction.method == GroupSortMethod::ByType) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->bean->DisplayType();
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->bean->DisplayType();
                              } else if (groupSortAction.method == GroupSortMethod::ByName) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->bean->name;
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->bean->name;
                              } else if (groupSortAction.method == GroupSortMethod::ByAddress) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->bean->DisplayAddress();
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->bean->DisplayAddress();
                              } else if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->full_test_report;
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->full_test_report;
                              }
                              auto get_latency_for_sort = [](int id) {
                                  auto i = ProxorGui::profileManager->GetProfile(id)->latency;
                                  if (i == 0) i = 100000;
                                  if (i < 0) i = 99999;
                                  return i;
                              };
                              if (groupSortAction.descending) {
                                  if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                      if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                          // compare latency if full_test_report is empty
                                          return get_latency_for_sort(a) > get_latency_for_sort(b);
                                      }
                                  }
                                  return ms_a > ms_b;
                              } else {
                                  if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                      auto int_a = ProxorGui::profileManager->GetProfile(a)->latency;
                                      auto int_b = ProxorGui::profileManager->GetProfile(b)->latency;
                                      if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                          // compare latency if full_test_report is empty
                                          return get_latency_for_sort(a) < get_latency_for_sort(b);
                                      }
                                  }
                                  return ms_a < ms_b;
                              }
                          });
                break;
            }
        }
        ui->proxyListTable->update_order(groupSortAction.save_sort);
    }

    // refresh data
    refresh_proxy_list_impl_refresh_data(id);
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const int &id) {
    auto group = ProxorGui::profileManager->CurrentGroup();
    auto toggleProxyIds = get_toggle_proxy_ids(group);
    QSignalBlocker blocker(ui->proxyListTable);
    // 绘制或更新item(s)
    for (int row = 0; row < ui->proxyListTable->rowCount(); row++) {
        auto profileId = ui->proxyListTable->row2Id[row];
        if (id >= 0 && profileId != id) continue; // refresh ONE item
        auto profile = ProxorGui::profileManager->GetProfile(profileId);
        if (profile == nullptr) continue;

        auto isRunning = profileId == ProxorGui::dataStore->started_id;
        auto f0 = std::make_unique<QTableWidgetItem>();
        f0->setData(114514, profileId);

        // Check state
        auto check = f0->clone();
        check->setText(isRunning ? "✓" : Int2String(row + 1));
        ui->proxyListTable->setVerticalHeaderItem(row, check);

        // C0: Toggle
        auto f = f0->clone();
        f->setText({});
        f->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        ui->proxyListTable->setItem(row, 0, f);
        ui->proxyListTable->setCellWidget(row, 0, makeCenteredCheckboxCell(
            ui->proxyListTable,
            toggleProxyIds.contains(profileId),
            [profileId, table = ui->proxyListTable](bool enabled) {
                auto group = ProxorGui::profileManager->CurrentGroup();
                if (group == nullptr) return;

                if (enabled) {
                    // Single-select: uncheck every other row visually
                    for (int i = 0; i < table->rowCount(); i++) {
                        auto *w = table->cellWidget(i, 0);
                        if (!w) continue;
                        auto *cb = w->findChild<QCheckBox *>();
                        if (cb && cb->isChecked()) {
                            auto *item = table->item(i, 0);
                            if (item && item->data(114514).toInt() != profileId) {
                                cb->blockSignals(true);
                                cb->setChecked(false);
                                cb->blockSignals(false);
                            }
                        }
                    }
                    group->toggle_proxy_ids = {profileId};
                } else {
                    group->toggle_proxy_ids.removeAll(profileId);
                }
                ProxorGui::profileManager->SaveGroup(group);
            }
        ));

        // C1: Name
        f = f0->clone();
        f->setText(profile->bean->name);
        if (isRunning) f->setForeground(palette().link());
        ui->proxyListTable->setItem(row, 1, f);

        // C2: Type
        f = f0->clone();
        f->setText(profile->bean->DisplayType());
        if (isRunning) f->setForeground(palette().link());
        ui->proxyListTable->setItem(row, 2, f);

        // C3: Address+Port
        f = f0->clone();
        f->setText(profile->bean->DisplayAddress());
        if (isRunning) f->setForeground(palette().link());
        ui->proxyListTable->setItem(row, 3, f);

        // C4: Test Result
        f = f0->clone();
        {
            auto color = profile->DisplayLatencyColor();
            if (color.isValid()) f->setForeground(color);
            f->setText(profile->DisplayLatency());
        }
        ui->proxyListTable->setItem(row, 4, f);

        // C5: Traffic
        f = f0->clone();
        f->setText(profile->traffic_data->DisplayTraffic());
        ui->proxyListTable->setItem(row, 5, f);

        // C6: Quota (subscription groups only)
        if (!ui->proxyListTable->isColumnHidden(6)) {
            f = f0->clone();
            f->setTextAlignment(Qt::AlignCenter);
            // Parse "upload=X; download=X; total=X" from group->info
            qint64 upload = 0, download = 0, total = 0;
            for (const auto &part : group->info.split(';')) {
                auto kv = part.trimmed().split('=');
                if (kv.size() != 2) continue;
                auto key = kv[0].trimmed();
                auto val = kv[1].trimmed().toLongLong();
                if (key == "upload") upload = val;
                else if (key == "download") download = val;
                else if (key == "total") total = val;
            }
            if (total > 0) {
                QString usedGiB = QString::number((double)(upload + download) / 1073741824.0, 'f', 2) + " GiB";
                QString totalGiB = QString::number((double)total / 1073741824.0, 'f', 2) + " GiB";
                f->setText(usedGiB + " / " + totalGiB);
            }
            ui->proxyListTable->setItem(row, 6, f);
        }
    }

    if (group != nullptr && !group->manually_column_width) {
        auto header = ui->proxyListTable->horizontalHeader();
        if (header->count() > 5) {
            ui->proxyListTable->resizeColumnToContents(5);
            const int trafficWidth = std::max(120, header->sectionSize(5));
            header->resizeSection(5, trafficWidth);
        }
    }
}

// table菜单相关

void MainWindow::on_proxyListTable_itemDoubleClicked(QTableWidgetItem *item) {
    if (item == nullptr || item->column() == 0) return;
    auto id = item->data(114514).toInt();
    if (select_mode) {
        emit profile_selected(id);
        select_mode = false;
        refresh_status();
        return;
    }
    auto dialog = new DialogEditProfile("", id, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_proxyListTable_itemChanged(QTableWidgetItem *item) {
    if (item == nullptr || item->column() != 0) return;

    auto group = ProxorGui::profileManager->CurrentGroup();
    if (group == nullptr) return;

    auto profileId = item->data(114514).toInt();

    if (item->checkState() == Qt::Checked) {
        // Single-select: uncheck every other row before updating the model
        ui->proxyListTable->blockSignals(true);
        for (int i = 0; i < ui->proxyListTable->rowCount(); i++) {
            auto *other = ui->proxyListTable->item(i, 0);
            if (other && other != item && other->checkState() == Qt::Checked)
                other->setCheckState(Qt::Unchecked);
        }
        ui->proxyListTable->blockSignals(false);

        group->toggle_proxy_ids = {profileId};
    } else {
        group->toggle_proxy_ids.removeAll(profileId);
    }

    ProxorGui::profileManager->SaveGroup(group);
}

void MainWindow::on_menu_add_from_input_triggered() {
    auto dialog = new DialogEditProfile("socks", ProxorGui::dataStore->current_group, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_clipboard_triggered() {
    auto clipboard = QApplication::clipboard()->text();
    ProxorGui_sub::groupUpdater->AsyncUpdate(clipboard);
}

void MainWindow::on_menu_clone_triggered() {
    auto ents = get_now_selected_list();
    if (ents.isEmpty()) return;

    auto btn = QMessageBox::question(this, tr("Clone"), tr("Clone %1 item(s)").arg(ents.count()));
    if (btn != QMessageBox::Yes) return;

    QStringList sls;
    for (const auto &ent: ents) {
        sls << ent->bean->ToProxorShareLink(ent->type);
    }

    ProxorGui_sub::groupUpdater->AsyncUpdate(sls.join("\n"));
}

void MainWindow::on_menu_move_triggered() {
    auto ents = get_now_selected_list();
    if (ents.isEmpty()) return;

    auto items = QStringList{};
    for (auto gid: ProxorGui::profileManager->groupsTabOrder) {
        auto group = ProxorGui::profileManager->GetGroup(gid);
        if (group == nullptr) continue;
        items += Int2String(gid) + " " + group->name;
    }

    bool ok;
    auto a = QInputDialog::getItem(nullptr,
                                   tr("Move"),
                                   tr("Move %1 item(s)").arg(ents.count()),
                                   items, 0, false, &ok);
    if (!ok) return;
    auto gid = SubStrBefore(a, " ").toInt();
    for (const auto &ent: ents) {
        ProxorGui::profileManager->MoveProfile(ent, gid);
    }
    refresh_proxy_list();
}

void MainWindow::on_menu_delete_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() == 0) return;
    if (QMessageBox::question(this, tr("Confirmation"), QString(tr("Remove %1 item(s) ?")).arg(ents.count())) ==
        QMessageBox::StandardButton::Yes) {
        for (const auto &ent: ents) {
            ProxorGui::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_reset_traffic_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() == 0) return;
    for (const auto &ent: ents) {
        ent->traffic_data->Reset();
        ProxorGui::profileManager->SaveProfile(ent);
        refresh_proxy_list(ent->id);
    }
}

void MainWindow::on_menu_profile_debug_info_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;
    auto btn = QMessageBox::information(this, software_name, ents.first()->ToJsonBytes(), "OK", "Edit", "Reload", 0, 0);
    if (btn == 1) {
        auto dialog = new DialogEditProfile("", ents.first()->id, this);
        connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
    } else if (btn == 2) {
        ProxorGui::dataStore->Load();
        ProxorGui::profileManager->LoadManager();
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_copy_links_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->copy();
        return;
    }
    auto ents = get_now_selected_list();
    QStringList links;
    for (const auto &ent: ents) {
        links += ent->bean->ToShareLink();
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    show_log_impl(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_copy_links_nkr_triggered() {
    auto ents = get_now_selected_list();
    QStringList links;
    for (const auto &ent: ents) {
        links += ent->bean->ToProxorShareLink(ent->type);
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    show_log_impl(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_export_config_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;
    auto ent = ents.first();
    if (ent->bean->DisplayCoreType() != software_core_name) return;

    auto result = BuildConfig(ent, false, true);
    QString config_core = QJsonObject2QString(result->coreConfig, false);
    QApplication::clipboard()->setText(config_core);

    QMessageBox msg(QMessageBox::Information, tr("Config copied"), tr("Config copied"));
    msg.addButton("Copy core config", QMessageBox::YesRole);
    msg.addButton("Copy test config", QMessageBox::NoRole);
    msg.addButton(QMessageBox::Ok);
    msg.setEscapeButton(QMessageBox::Ok);
    msg.setDefaultButton(QMessageBox::Ok);
    auto ret = msg.exec();
    if (ret == 2) {
        result = BuildConfig(ent, false, false);
        config_core = QJsonObject2QString(result->coreConfig, false);
        QApplication::clipboard()->setText(config_core);
    } else if (ret == 3) {
        result = BuildConfig(ent, true, false);
        config_core = QJsonObject2QString(result->coreConfig, false);
        QApplication::clipboard()->setText(config_core);
    }
}

void MainWindow::display_qr_link(bool nkrFormat) {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;

    class W : public QDialog {
    public:
        QLabel *l = nullptr;
        QCheckBox *cb = nullptr;
        //
        QPlainTextEdit *l2 = nullptr;
        QImage im;
        //
        QString link;
        QString link_nk;

        void show_qr(const QSize &size) const {
            auto side = size.height() - 20 - l2->size().height() - cb->size().height();
            l->setPixmap(QPixmap::fromImage(im.scaled(side, side, Qt::KeepAspectRatio, Qt::FastTransformation),
                                            Qt::MonoOnly));
            l->resize(side, side);
        }

        void refresh(bool is_nk) {
            auto link_display = is_nk ? link_nk : link;
            l2->setPlainText(link_display);
            constexpr qint32 qr_padding = 2;
            //
            try {
                qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(link_display.toUtf8().data(), qrcodegen::QrCode::Ecc::MEDIUM);
                qint32 sz = qr.getSize();
                im = QImage(sz + qr_padding * 2, sz + qr_padding * 2, QImage::Format_RGB32);
                QRgb black = qRgb(0, 0, 0);
                QRgb white = qRgb(255, 255, 255);
                im.fill(white);
                for (int y = 0; y < sz; y++)
                    for (int x = 0; x < sz; x++)
                        if (qr.getModule(x, y))
                            im.setPixel(x + qr_padding, y + qr_padding, black);
                show_qr(size());
            } catch (const std::exception &ex) {
                QMessageBox::warning(nullptr, "error", ex.what());
            }
        }

        W(const QString &link_, const QString &link_nk_) {
            link = link_;
            link_nk = link_nk_;
            //
            setLayout(new QVBoxLayout);
            setMinimumSize(256, 256);
            QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            sizePolicy.setHeightForWidth(true);
            setSizePolicy(sizePolicy);
            //
            l = new QLabel();
            l->setMinimumSize(256, 256);
            l->setMargin(6);
            l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            l->setScaledContents(true);
            layout()->addWidget(l);
            cb = new QCheckBox;
            cb->setText("Proxor Links");
            layout()->addWidget(cb);
            l2 = new QPlainTextEdit();
            l2->setReadOnly(true);
            layout()->addWidget(l2);
            //
            connect(cb, &QCheckBox::toggled, this, &W::refresh);
            refresh(false);
        }

        void resizeEvent(QResizeEvent *resizeEvent) override {
            show_qr(resizeEvent->size());
        }
    };

    auto link = ents.first()->bean->ToShareLink();
    auto proxorLink = ents.first()->bean->ToProxorShareLink(ents.first()->type);
    auto w = new W(link, proxorLink);
    w->setWindowTitle(ents.first()->bean->DisplayTypeAndName());
    w->exec();
    w->deleteLater();
}

void MainWindow::on_menu_scan_qr_triggered() {
#ifndef NKR_NO_ZXING
    using namespace ZXingQt;

    hide();
    QThread::sleep(1);

    auto screen = QGuiApplication::primaryScreen();
    auto geom = screen->geometry();
    auto qpx = screen->grabWindow(0, geom.x(), geom.y(), geom.width(), geom.height());

    show();

    auto hints = DecodeHints()
                     .setFormats(BarcodeFormat::QRCode)
                     .setTryRotate(false)
                     .setBinarizer(Binarizer::FixedThreshold);

    auto result = ReadBarcode(qpx.toImage(), hints);
    const auto &text = result.text();
    if (text.isEmpty()) {
        MessageBoxInfo(software_name, tr("QR Code not found"));
    } else {
        show_log_impl("QR Code Result:\n" + text);
        ProxorGui_sub::groupUpdater->AsyncUpdate(text);
    }
#endif
}

void MainWindow::on_menu_clear_test_result_triggered() {
    for (const auto &profile: get_selected_or_group()) {
        profile->latency = 0;
        profile->full_test_report = "";
        ProxorGui::profileManager->SaveProfile(profile);
    }
    refresh_proxy_list();
}

void MainWindow::on_menu_select_all_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->selectAll();
        return;
    }
    ui->proxyListTable->selectAll();
}

void MainWindow::on_menu_add_subscription_triggered() {
    auto ent = ProxorGui::ProfileManager::NewGroup();
    ent->url = " "; // non-empty: forces DialogEditGroup to show Subscription type with URL field
    auto dialog = new DialogEditGroup(ent, this);
    connect(dialog, &QDialog::finished, this, [=] {
        if (dialog->result() == QDialog::Accepted) {
            ProxorGui::profileManager->AddGroup(ent);
            refresh_groups();
        }
        dialog->deleteLater();
    });
    dialog->show();
}

void MainWindow::on_menu_delete_repeat_triggered() {
    QList<std::shared_ptr<ProxorGui::ProxyEntity>> out;
    QList<std::shared_ptr<ProxorGui::ProxyEntity>> out_del;

    ProxorGui::ProfileFilter::Uniq(ProxorGui::profileManager->CurrentGroup()->Profiles(), out, true, false);
    ProxorGui::ProfileFilter::OnlyInSrc_ByPointer(ProxorGui::profileManager->CurrentGroup()->Profiles(), out, out_del);

    int remove_display_count = 0;
    QString remove_display;
    for (const auto &ent: out_del) {
        remove_display += ent->bean->DisplayTypeAndName() + "\n";
        if (++remove_display_count == 20) {
            remove_display += "...";
            break;
        }
    }

    if (out_del.length() > 0 &&
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            ProxorGui::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

bool mw_sub_updating = false;

void MainWindow::on_menu_update_subscription_triggered() {
    auto group = ProxorGui::profileManager->CurrentGroup();
    if (group->url.isEmpty()) return;
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    ProxorGui_sub::groupUpdater->AsyncUpdate(group->url, group->id, [&] { mw_sub_updating = false; });
}

void MainWindow::on_menu_remove_unavailable_triggered() {
    QList<std::shared_ptr<ProxorGui::ProxyEntity>> out_del;

    for (const auto &[_, profile]: ProxorGui::profileManager->profiles) {
        if (ProxorGui::dataStore->current_group != profile->gid) continue;
        if (profile->latency < 0) out_del += profile;
    }

    int remove_display_count = 0;
    QString remove_display;
    for (const auto &ent: out_del) {
        remove_display += ent->bean->DisplayTypeAndName() + "\n";
        if (++remove_display_count == 20) {
            remove_display += "...";
            break;
        }
    }

    if (out_del.length() > 0 &&
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            ProxorGui::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_resolve_domain_triggered() {
    auto profiles = get_selected_or_group();
    if (profiles.isEmpty()) return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Resolving domain to IP, if support.")) != QMessageBox::StandardButton::Yes) {
        return;
    }
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    ProxorGui::dataStore->resolve_count = profiles.count();

    for (const auto &profile: profiles) {
        profile->bean->ResolveDomainToIP([=] {
            ProxorGui::profileManager->SaveProfile(profile);
            if (--ProxorGui::dataStore->resolve_count != 0) return;
            refresh_proxy_list();
            mw_sub_updating = false;
        });
    }
}

void MainWindow::on_proxyListTable_customContextMenuRequested(const QPoint &pos) {
    ui->menu_server->popup(ui->proxyListTable->viewport()->mapToGlobal(pos)); // 弹出菜单
}

QList<std::shared_ptr<ProxorGui::ProxyEntity>> MainWindow::get_now_selected_list() {
    auto items = ui->proxyListTable->selectedItems();
    QList<std::shared_ptr<ProxorGui::ProxyEntity>> list;
    for (auto item: items) {
        auto id = item->data(114514).toInt();
        auto ent = ProxorGui::profileManager->GetProfile(id);
        if (ent != nullptr && !list.contains(ent)) list += ent;
    }
    return list;
}

QList<std::shared_ptr<ProxorGui::ProxyEntity>> MainWindow::get_selected_or_group() {
    auto selected_or_group = ui->menu_server->property("selected_or_group").toInt();
    QList<std::shared_ptr<ProxorGui::ProxyEntity>> profiles;
    if (selected_or_group > 0) {
        profiles = get_now_selected_list();
        if (profiles.isEmpty() && selected_or_group == 2) profiles = ProxorGui::profileManager->CurrentGroup()->ProfilesWithOrder();
    } else {
        profiles = ProxorGui::profileManager->CurrentGroup()->ProfilesWithOrder();
    }
    return profiles;
}

QList<int> MainWindow::get_toggle_proxy_ids(const std::shared_ptr<ProxorGui::Group> &group) const {
    if (group == nullptr) return {};

    QList<int> validIds;
    for (const auto &profile: group->ProfilesWithOrder()) {
        if (group->toggle_proxy_ids.contains(profile->id)) {
            validIds << profile->id;
        }
    }
    return validIds;
}

void MainWindow::on_toolButton_toggle_proxy_clicked() {
    if (ProxorGui::dataStore->started_id >= 0) {
        proxor_stop();
        return;
    }

    auto group = ProxorGui::profileManager->CurrentGroup();
    if (group == nullptr || group->archive) return;

    auto toggleProxyIds = get_toggle_proxy_ids(group);
    if (toggleProxyIds.isEmpty()) {
        MessageBoxWarning(software_name, tr("Select at least one proxy in the Toggle column first."));
        return;
    }

    proxor_start(toggleProxyIds.first());
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            // take over by shortcut_esc
            break;
        case Qt::Key_Enter:
            proxor_start();
            break;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

// Log

// Map an ANSI SGR parameter string to a QColor.
// Handles standard colors (30-37, 90-97) and 256-color "38;5;N".
static QColor ansiParamToColor(const QString &param) {
    static const QColor std16[] = {
        {85,85,85},{204,0,0},{0,204,0},{204,204,0},{0,0,204},{204,0,204},{0,204,204},{204,204,204},
        {136,136,136},{255,68,68},{68,255,68},{255,255,68},{68,68,255},{255,68,255},{68,255,255},{255,255,255},
    };
    if (param.startsWith(QStringLiteral("38;5;"))) {
        bool ok = false;
        int n = param.mid(5).toInt(&ok);
        if (!ok || n < 0 || n > 255) return {};
        if (n < 16) return std16[n];
        if (n < 232) {
            n -= 16;
            int b = n % 6; n /= 6;
            int g = n % 6; n /= 6;
            int r = n;
            auto c = [](int v) { return v == 0 ? 0 : 55 + v * 40; };
            return {c(r), c(g), c(b)};
        }
        int v = 8 + (n - 232) * 10;
        return {v, v, v};
    }
    bool ok = false;
    int code = param.toInt(&ok);
    if (!ok) return {};
    if (code >= 30 && code <= 37) return std16[code - 30];
    if (code >= 90 && code <= 97) return std16[code - 90 + 8];
    return {};
}

static void applyAnsiParams(const QString &param, QColor &foreground) {
    if (param.isEmpty()) {
        foreground = {};
        return;
    }

    const auto parts = param.split(';', Qt::KeepEmptyParts);
    for (int i = 0; i < parts.size(); ++i) {
        bool ok = false;
        const int code = parts[i].toInt(&ok);
        if (!ok) continue;

        switch (code) {
        case 0:
            foreground = {};
            break;
        case 39:
            foreground = {};
            break;
        case 38:
            if (i + 2 < parts.size() && parts[i + 1] == u"5") {
                QColor col = ansiParamToColor(QStringLiteral("38;5;") + parts[i + 2]);
                if (col.isValid()) foreground = col;
                i += 2;
            }
            break;
        default: {
            QColor col = ansiParamToColor(parts[i]);
            if (col.isValid()) foreground = col;
            break;
        }
        }
    }
}

// Append one log line (may contain ANSI escape codes) to the document with color.
static void appendAnsiLine(const QString &line, QTextDocument *doc) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    if (!doc->isEmpty()) {
        cursor.insertBlock();
    }

    QColor foreground;
    int i = 0;
    QString seg;
    QString html;

    auto flush = [&]() {
        if (seg.isEmpty()) return;
        const auto escaped = seg.toHtmlEscaped();
        bool useSpan = foreground.isValid();
        QColor renderColor = foreground;

        if (useSpan && qApp) {
            bool isDarkTheme = qApp->palette().color(QPalette::Base).lightness() < 128;
            if (!isDarkTheme && renderColor.lightness() > 170) {
                useSpan = false; // Fallback to native black
            } else if (isDarkTheme && renderColor.lightness() < 80) {
                useSpan = false; // Fallback to native white
            }
        }

        if (useSpan) {
            html += QStringLiteral("<span style=\"color:%1;\">%2</span>")
                        .arg(renderColor.name(QColor::HexRgb), escaped);
        } else {
            html += escaped;
        }
        seg.clear();
    };

    while (i < line.size()) {
        if (line[i] == u'\x1b' && i + 1 < line.size() && line[i + 1] == u'[') {
            int j = i + 2;
            while (j < line.size() && line[j] != u'm') ++j;
            if (j < line.size()) {
                flush();
                QString param = line.mid(i + 2, j - i - 2);
                applyAnsiParams(param, foreground);
                i = j + 1;
                continue;
            }
        }
        seg += line[i++];
    }
    flush();
    cursor.insertHtml(html);
    cursor.endEditBlock();
}

void MainWindow::rebuildLogDocument(const QString &filter) {
    qvLogDocument->clear();
    const bool hasFilter = !filter.isEmpty();
    for (const auto &line : m_logLines) {
        if (hasFilter && !line.contains(filter, Qt::CaseInsensitive)) continue;
        appendAnsiLine(line, qvLogDocument);
    }
}

void MainWindow::show_log_impl(const QString &log) {
    auto lines = SplitLines(log.trimmed());
    if (lines.isEmpty()) return;

    QStringList newLines;
    auto log_ignore = ProxorGui::dataStore->log_ignore;
    for (const auto &line: lines) {
        bool showThisLine = true;
        for (const auto &str: log_ignore) {
            if (line.contains(str)) {
                showThisLine = false;
                break;
            }
        }
        if (showThisLine) newLines << line;
    }
    if (newLines.isEmpty()) return;

    // Append to buffer
    for (const auto &line : newLines) {
        m_logLines.append(line);
    }
    while (m_logLines.size() > ProxorGui::dataStore->max_log_line) {
        m_logLines.removeFirst();
    }

    // Append to document (respecting active filter)
    const QString filterText = ui->log_filter->text();
    for (const auto &line : newLines) {
        if (!filterText.isEmpty() && !line.contains(filterText, Qt::CaseInsensitive))
            continue;
        appendAnsiLine(line, qvLogDocument);
    }
    // Trim document to max_log_line when no filter is active
    if (filterText.isEmpty()) {
        // From https://gist.github.com/jemyzhang/7130092
        auto block = qvLogDocument->begin();
        while (block.isValid()) {
            if (qvLogDocument->blockCount() > ProxorGui::dataStore->max_log_line) {
                QTextCursor cursor(block);
                block = block.next();
                cursor.select(QTextCursor::BlockUnderCursor);
                cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
                continue;
            }
            break;
        }
    }
}

#define ADD_TO_CURRENT_ROUTE(a, b)                                                                   \
    ProxorGui::dataStore->routing->a = (SplitLines(ProxorGui::dataStore->routing->a) << (b)).join("\n"); \
    ProxorGui::dataStore->routing->Save();

void MainWindow::on_masterLogBrowser_customContextMenuRequested(const QPoint &pos) {
    QMenu *menu = ui->masterLogBrowser->createStandardContextMenu();

    auto sep = new QAction(this);
    sep->setSeparator(true);
    menu->addAction(sep);

    auto action_add_ignore = new QAction(this);
    action_add_ignore->setText(tr("Set ignore keyword"));
    connect(action_add_ignore, &QAction::triggered, this, [=] {
        auto list = ProxorGui::dataStore->log_ignore;
        auto newStr = ui->masterLogBrowser->textCursor().selectedText().trimmed();
        if (!newStr.isEmpty()) list << newStr;
        bool ok;
        newStr = QInputDialog::getMultiLineText(GetMessageBoxParent(), tr("Set ignore keyword"), tr("Set the following keywords to ignore?\nSplit by line."), list.join("\n"), &ok);
        if (ok) {
            ProxorGui::dataStore->log_ignore = SplitLines(newStr);
            ProxorGui::dataStore->Save();
        }
    });
    menu->addAction(action_add_ignore);

    auto action_add_route = new QAction(this);
    action_add_route->setText(tr("Save as route"));
    connect(action_add_route, &QAction::triggered, this, [=] {
        auto newStr = ui->masterLogBrowser->textCursor().selectedText().trimmed();
        if (newStr.isEmpty()) return;
        //
        bool ok;
        newStr = QInputDialog::getText(GetMessageBoxParent(), tr("Save as route"), tr("Edit"), {}, newStr, &ok).trimmed();
        if (!ok) return;
        if (newStr.isEmpty()) return;
        //
        auto select = IsIpAddress(newStr) ? 0 : 3;
        QStringList items = {"proxyIP", "bypassIP", "blockIP", "proxyDomain", "bypassDomain", "blockDomain"};
        auto item = QInputDialog::getItem(GetMessageBoxParent(), tr("Save as route"),
                                          tr("Save \"%1\" as a routing rule?").arg(newStr),
                                          items, select, false, &ok);
        if (ok) {
            auto index = items.indexOf(item);
            switch (index) {
                case 0:
                    ADD_TO_CURRENT_ROUTE(proxy_ip, newStr);
                    break;
                case 1:
                    ADD_TO_CURRENT_ROUTE(direct_ip, newStr);
                    break;
                case 2:
                    ADD_TO_CURRENT_ROUTE(block_ip, newStr);
                    break;
                case 3:
                    ADD_TO_CURRENT_ROUTE(proxy_domain, newStr);
                    break;
                case 4:
                    ADD_TO_CURRENT_ROUTE(direct_domain, newStr);
                    break;
                case 5:
                    ADD_TO_CURRENT_ROUTE(block_domain, newStr);
                    break;
                default:
                    break;
            }
            MW_dialog_message("", "UpdateDataStore,RouteChanged");
        }
    });
    menu->addAction(action_add_route);

    auto action_clear = new QAction(this);
    action_clear->setText(tr("Clear"));
    connect(action_clear, &QAction::triggered, this, [=] {
        qvLogDocument->clear();
        ui->masterLogBrowser->clear();
        m_logLines.clear();
    });
    menu->addAction(action_clear);

    menu->exec(ui->masterLogBrowser->viewport()->mapToGlobal(pos)); // 弹出菜单
}

// eventFilter

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (obj == ui->label_running && mouseEvent->button() == Qt::LeftButton && running != nullptr) {
            speedtest_current();
            return true;
        } else if (obj == ui->label_inbound && mouseEvent->button() == Qt::LeftButton) {
            on_menu_basic_settings_triggered();
            return true;
        }
    } else if (event->type() == QEvent::MouseButtonDblClick) {
        if (obj == ui->splitter) {
            auto size = ui->splitter->size();
            ui->splitter->setSizes({size.height() / 2, size.height() / 2});
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// profile selector

void MainWindow::start_select_mode(QObject *context, const std::function<void(int)> &callback) {
    select_mode = true;
    connectOnce(this, &MainWindow::profile_selected, context, callback);
    refresh_status();
}

// 连接列表

inline QJsonArray last_arr; // Matches the gRPC connection statistics payload.

void MainWindow::refresh_connection_list(const QJsonArray &arr) {
    if (last_arr == arr) {
        return;
    }
    last_arr = arr;
    ui->tableWidget_conn->setSortingEnabled(false);

    if (ProxorGui::dataStore->flag_debug) qDebug() << arr;

    ui->tableWidget_conn->setRowCount(0);

    int row = -1;
    for (const auto &_item: arr) {
        auto item = _item.toObject();
        if (ProxorGui::dataStore->ignoreConnTag.contains(item["Tag"].toString())) continue;

        row++;
        ui->tableWidget_conn->insertRow(row);

        auto f0 = std::make_unique<QTableWidgetItem>();
        f0->setData(114514, item["ID"].toInt());

        // C0: Status
        auto c0 = new QLabel;
        auto start_t = item["Start"].toInt();
        auto end_t = item["End"].toInt();
        // icon
        auto outboundTag = item["Tag"].toString();
        if (outboundTag == "block") {
            c0->setPixmap(Icon::GetMaterialIcon("cancel"));
        } else {
            if (end_t > 0) {
                c0->setPixmap(Icon::GetMaterialIcon("history"));
            } else {
                c0->setPixmap(Icon::GetMaterialIcon("swap-vertical"));
            }
        }
        c0->setAlignment(Qt::AlignCenter);
        c0->setToolTip(tr("Start: %1\nEnd: %2").arg(DisplayTime(start_t), end_t > 0 ? DisplayTime(end_t) : ""));
        ui->tableWidget_conn->setCellWidget(row, 0, c0);
        auto f_status = f0->clone();
        f_status->setData(Qt::DisplayRole, static_cast<int>(item["Start"].toVariant().toLongLong()));
        ui->tableWidget_conn->setItem(row, 0, f_status);

        // C1: Outbound
        auto f = f0->clone();
        f->setToolTip("");
        f->setText(outboundTag);
        ui->tableWidget_conn->setItem(row, 1, f);

        // C2: Destination
        f = f0->clone();
        QString target1 = item["Dest"].toString();
        QString target2 = item["RDest"].toString();
        if (target2.isEmpty() || target1 == target2) {
            target2 = "";
        }
        f->setText("[" + target1 + "] " + target2);
        ui->tableWidget_conn->setItem(row, 2, f);

        // C3: Process
        f = f0->clone();
        const auto process = item["Process"].toString();
        f->setText(process.isEmpty() ? QStringLiteral("-") : process);
        ui->tableWidget_conn->setItem(row, 3, f);

        // C4: Protocol
        f = f0->clone();
        auto protocol = item["Network"].toString();
        const auto protocolDetail = item["Protocol"].toString();
        if (!protocolDetail.isEmpty()) {
            protocol += " (" + protocolDetail + ")";
        }
        f->setText(protocol);
        ui->tableWidget_conn->setItem(row, 4, f);

        // C5: Traffic
        f = f0->clone();
        const auto upload = item["Upload"].toVariant().toLongLong();
        const auto download = item["Download"].toVariant().toLongLong();
        f->setText(ReadableSize(upload) + "↑ " + ReadableSize(download) + "↓");
        ui->tableWidget_conn->setItem(row, 5, f);
    }
    ui->tableWidget_conn->setSortingEnabled(true);
    // Re-apply active connection filter after refresh
    const QString connFilterText = ui->conn_filter->text();
    if (!connFilterText.isEmpty()) {
        const int rows = ui->tableWidget_conn->rowCount();
        for (int r = 0; r < rows; ++r) {
            bool match = false;
            for (int c = 1; c <= 4; ++c) {
                auto *itm = ui->tableWidget_conn->item(r, c);
                if (itm && itm->text().contains(connFilterText, Qt::CaseInsensitive)) {
                    match = true;
                    break;
                }
            }
            ui->tableWidget_conn->setRowHidden(r, !match);
        }
    }
}

// Hotkey

#ifndef NKR_NO_QHOTKEY

#include <QHotkey>

inline QList<std::shared_ptr<QHotkey>> RegisteredHotkey;

void MainWindow::RegisterHotkey(bool unregister) {
    while (!RegisteredHotkey.isEmpty()) {
        auto hk = RegisteredHotkey.takeFirst();
        hk->deleteLater();
    }
    if (unregister) return;

    QStringList regstr{
        ProxorGui::dataStore->hotkey_mainwindow,
        ProxorGui::dataStore->hotkey_group,
        ProxorGui::dataStore->hotkey_route,
        ProxorGui::dataStore->hotkey_system_proxy_menu,
    };

    for (const auto &key: regstr) {
        if (key.isEmpty()) continue;
        if (regstr.count(key) > 1) return; // Conflict hotkey
    }
    for (const auto &key: regstr) {
        QKeySequence k(key);
        if (k.isEmpty()) continue;
        auto hk = std::make_shared<QHotkey>(k, true);
        if (hk->isRegistered()) {
            RegisteredHotkey += hk;
            connect(hk.get(), &QHotkey::activated, this, [=] { HotkeyEvent(key); });
        } else {
            hk->deleteLater();
        }
    }
}

void MainWindow::HotkeyEvent(const QString &key) {
    if (key.isEmpty()) return;
    runOnUiThread([=] {
        if (key == ProxorGui::dataStore->hotkey_mainwindow) {
            tray->activated(QSystemTrayIcon::ActivationReason::Trigger);
        } else if (key == ProxorGui::dataStore->hotkey_group) {
            on_menu_manage_groups_triggered();
        } else if (key == ProxorGui::dataStore->hotkey_route) {
            on_menu_routing_settings_triggered();
        } else if (key == ProxorGui::dataStore->hotkey_system_proxy_menu) {
            ui->menu_spmode->popup(QCursor::pos());
        }
    });
}

#else

void MainWindow::RegisterHotkey(bool unregister) {}

void MainWindow::HotkeyEvent(const QString &key) {}

#endif

// VPN Launcher

bool MainWindow::StartVPNProcess() {
    //
    if (vpn_pid != 0) {
        return true;
    }
    //
    auto configPath = ProxorGui::WriteVPNSingBoxConfig();
    auto scriptPath = ProxorGui::WriteVPNLinuxScript(configPath);
    //
#ifdef Q_OS_WIN
    runOnNewThread([=] {
        vpn_pid = 1; // TODO get pid?
        WinCommander::runProcessElevated(ProxorGui::PackageExecutablePath("proxor_core"),
                                         {"--disable-color", "run", "-c", configPath}, "",
                                         ProxorGui::dataStore->vpn_hide_console ? WinCommander::SW_HIDE : WinCommander::SW_SHOWMINIMIZED); // blocking
        vpn_pid = 0;
        runOnUiThread([=] { proxor_set_spmode_vpn(false); });
    });
#else
    //
    auto vpn_process = new QProcess;
    QProcess::connect(vpn_process, &QProcess::stateChanged, this, [=](QProcess::ProcessState state) {
        if (state == QProcess::NotRunning) {
            vpn_pid = 0;
            vpn_process->deleteLater();
            GetMainWindow()->proxor_set_spmode_vpn(false);
        }
    });
    //
    vpn_process->setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_MACOS
    vpn_process->start("osascript", {"-e", QStringLiteral("do shell script \"%1\" with administrator privileges")
                                               .arg("bash " + scriptPath)});
#else
    vpn_process->start("pkexec", {"bash", scriptPath});
#endif
    vpn_process->waitForStarted();
    vpn_pid = vpn_process->processId(); // actually it's pkexec or bash PID
#endif
    return true;
}

bool MainWindow::StopVPNProcess(bool unconditional) {
    if (unconditional || vpn_pid != 0) {
        bool ok;
        core_process->processId();
#ifdef Q_OS_WIN
        auto ret = WinCommander::runProcessElevated("taskkill", {"/IM", "proxor_core.exe",
                                                                 "/FI",
                                                                 "PID ne " + Int2String(core_process->processId())});
        ok = ret == 0;
#else
        QProcess p;
#ifdef Q_OS_MACOS
        p.start("osascript", {"-e", QStringLiteral("do shell script \"%1\" with administrator privileges")
                                        .arg("pkill -2 -U 0 proxor_core")});
#else
        if (unconditional) {
            p.start("pkexec", {"killall", "-2", "proxor_core"});
        } else {
            p.start("pkexec", {"pkill", "-2", "-P", Int2String(vpn_pid)});
        }
#endif
        p.waitForFinished();
        ok = p.exitCode() == 0;
#endif
        if (!unconditional) {
            ok ? vpn_pid = 0 : MessageBoxWarning(tr("Error"), tr("Failed to stop Tun process"));
        }
        return ok;
    }
    return true;
}
