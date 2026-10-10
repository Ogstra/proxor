#include "./ui_mainwindow.h"
#include "sys/LogFile.hpp"
#include "mainwindow.h"

#include "fmt/Preset.hpp"
#include "db/ProfileFilter.hpp"
#include "db/ConfigBuilder.hpp"
#include "sub/GroupUpdater.hpp"
#include "sys/ExternalProcess.hpp"
#include "sys/WifiMonitor.hpp"
#include "sys/wifi/WifiBackend.hpp"
#include "sys/wifi/WifiPermission.hpp"
#include "main/PackagePolicy.hpp"

#include "ui/ThemeManager.hpp"
#include "ui/Icon.hpp"
#include "ui/widget/ProxyListFilterHeader.h"
#include "ui/edit/dialog_edit_profile.h"
#include "ui/edit/dialog_edit_group.h"
#include "ui/dialog_basic_settings.h"
#include "ui/dialog_manage_groups.h"
#include "ui/dialog_manage_routes.h"
#include "ui/dialog_vpn_settings.h"
#include "ui/dialog_ssid_settings.h"
#include "ui/dialog_hotkey.h"
#ifdef Q_OS_WIN
#include "ui/dialog_update_available.h"
#endif
#ifdef Q_OS_MACOS
#include "ui/dialog_update_available.h"
#include <QSaveFile>
#endif
#include "platform/PlatformCapabilitiesApp.hpp"
#include "platform/HotkeyReport.hpp"
#include "platform/QrScanPolicy.hpp"
#if (defined(Q_OS_MACOS) || defined(NKR_QT_CAMERA)) && !defined(NKR_NO_ZXING)
#define NKR_CAMERA_SCAN
#include "ui/dialog_scan_camera.h"
#endif
#include "platform/QrImageDecode.hpp"
#include "platform/LinuxSystemProxyPlan.hpp"
#include "platform/TrayPolicy.hpp"
#include "platform/PortalShortcutTrigger.hpp"
#include "sys/DesktopPortal.hpp"

#include "3rdparty/fix_old_qt.h"
#include "3rdparty/qrcodegen.hpp"
#include "3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.hpp"


#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include "sys/windows/ActiveNetworkType.h"
#else
#ifdef Q_OS_LINUX
#include "sys/linux/LinuxCap.h"
#endif
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#endif

#ifdef Q_OS_MACOS
#include "ui/mac/MacPlatform.h"
#include "platform/MacReopenPolicy.hpp"
#include "ui/mac/MacLook.h"
#include "ui/mac/MacDialogs.h"
#include "sys/macos/MacHelperClient.h"
#include "sys/macos/MacHelperService.h"
#include "sys/macos/MacHelperInstaller.h"
#include "sys/macos/MacLocalNetwork.h"
#include "platform/TraySpeed.hpp"
#include "platform/LocalNetworksApp.hpp"
#include "sys/macos/MacScreenCapture.h"
#endif

#include <QClipboard>
#include <QFileDialog>
#include <QMimeData>
#include <QImageReader>
#include <QApplication>
#include <QAbstractItemView>
#include <QBrush>
#include <QColor>
#include <QDateTime>
#include <QLabel>
#include <QCheckBox>
#include <QFontMetrics>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTableWidgetItem>
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
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFileInfo>
#include <QPainter>
#include <QMouseEvent>
#include <QStyleHints>
#include <QNetworkInformation>
#include <QProcess>
#include <QSet>
#include <QTextStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QUrlQuery>
#include <QSysInfo>
#include <QPushButton>

namespace {
#ifndef Q_OS_MACOS
// Top-bar menu buttons take Tab focus; Space already clicks a button, this adds Enter/Return.
class EnterClicksFilter : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            const int key = static_cast<QKeyEvent *>(event)->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter) {
                if (auto *button = qobject_cast<QAbstractButton *>(watched)) {
                    button->click();
                    return true;
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
#endif

// When the tunnel is the active mode, every test leaves through it, so a test that runs
// while the core is still installing routes reports the whole list as unavailable. The
// moment Tun was last switched on lives here rather than in the class because the tests
// only need to know how long the routes have had to settle.
qint64 g_tun_enabled_ms = 0;
constexpr qint64 kTunSettleMs = 6000;

// Qt::SingleShotConnection retires the connection on the FIRST emission of the signal,
// whether or not the slot did anything useful. reachabilityChanged fires for every
// topology change -- notably when the TUN adapter comes up -- so a slot that filters for
// Online was being disconnected by an unrelated transition and never saw the real one.
// Disconnect only once the callback has actually run.
void runOnceWhenOnline(QNetworkInformation *ni, QObject *context, std::function<void()> fn) {
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = QObject::connect(ni, &QNetworkInformation::reachabilityChanged, context,
        [conn, fn = std::move(fn)](QNetworkInformation::Reachability r) {
            if (r != QNetworkInformation::Reachability::Online) return;
            QObject::disconnect(*conn);
            fn();
        });
}

constexpr int kAddGroupTabId = -114514;
constexpr auto kProjectOwner = "Ogstra";
constexpr auto kProjectRepo = "proxor";

QString projectRepoSlug() {
    return QStringLiteral("%1/%2").arg(QString::fromLatin1(kProjectOwner), QString::fromLatin1(kProjectRepo));
}

QUrl projectUrl(const QString &path = QString()) {
    return QUrl(QStringLiteral("https://github.com/%1%2").arg(projectRepoSlug(), path));
}

QString versionInfoText() {
    return QStringLiteral("%1 %2\nQt: %3\nOS: %4\nRepository: %5")
        .arg(software_name,
             QStringLiteral(NKR_VERSION),
             QString::fromLatin1(qVersion()),
             QSysInfo::prettyProductName(),
             projectUrl().toString());
}

QUrl bugReportUrl() {
    QUrl url = projectUrl(QStringLiteral("/issues/new"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("title"), QStringLiteral("Bug: "));
    query.addQueryItem(QStringLiteral("body"), versionInfoText() + QStringLiteral("\n\nDescribe the issue:\n"));
    url.setQuery(query);
    return url;
}

QUrl sponsorUrl() {
    return QUrl(QStringLiteral("https://github.com/sponsors/%1").arg(QString::fromLatin1(kProjectOwner)));
}

QUrl maintainerUrl() {
    return QUrl(QStringLiteral("https://github.com/%1").arg(QString::fromLatin1(kProjectOwner)));
}

#ifdef Q_OS_WIN
constexpr auto kProxorHostsBegin = "# BEGIN PROXOR HOSTS";
constexpr auto kProxorHostsEnd = "# END PROXOR HOSTS";

QString windowsHostsPath() {
    const auto systemRoot = qEnvironmentVariable("SystemRoot", "C:\\Windows");
    return QDir::fromNativeSeparators(systemRoot + "\\System32\\drivers\\etc\\hosts");
}

bool isValidHostsLabel(const QString &host) {
    if (host.isEmpty() || host.size() > 63 || host.startsWith('-') || host.endsWith('-')) return false;
    for (const auto ch: host) {
        if (!(ch.isLetterOrNumber() || ch == '-')) return false;
    }
    return true;
}

QString removeProxorHostsBlock(QString text) {
    text.replace("\r\n", "\n").replace('\r', '\n');
    QStringList kept;
    bool inBlock = false;
    for (const auto &line: text.split('\n')) {
        const auto trimmed = line.trimmed();
        if (trimmed == kProxorHostsBegin) {
            inBlock = true;
            continue;
        }
        if (trimmed == kProxorHostsEnd) {
            inBlock = false;
            continue;
        }
        if (!inBlock) kept += line;
    }
    while (!kept.isEmpty() && kept.last().trimmed().isEmpty()) kept.removeLast();
    return kept.join("\r\n");
}

QString buildProxorHostsBlock() {
    if (ProxorGui::dataStore == nullptr || ProxorGui::dataStore->routing == nullptr) return {};

    QStringList entries;
    QSet<QString> seen;
    const QString currentSsid = WifiMonitor::cachedSsid();
    for (const auto &line: SplitLinesSkipSharp(ProxorGui::dataStore->routing->hosts_mapping)) {
        const auto parts = line.simplified().split(' ', Qt::SkipEmptyParts);
        if (parts.size() < 2) continue;

        auto host = parts[0].trimmed().toLower();
        const auto ip = parts[1].trimmed();
        if (host.endsWith('.')) host.chop(1);
        if (!host.contains('.') && isValidHostsLabel(host) && IsIpAddress(ip)) {
            if (parts.size() >= 3 && !currentSsid.isEmpty()) {
                bool skip = false;
                for (const auto &ssid: parts[2].split(',', Qt::SkipEmptyParts)) {
                    if (ssid.trimmed() == currentSsid) {
                        skip = true;
                        break;
                    }
                }
                if (skip) continue;
            }

            const auto key = host + "=" + ip;
            if (seen.contains(key)) continue;
            seen += key;
            entries += ip + "\t" + host;
        }
    }

    if (entries.isEmpty()) return {};
    return QString(kProxorHostsBegin) + "\r\n" + entries.join("\r\n") + "\r\n" + QString(kProxorHostsEnd);
}

bool writeTextFile(const QString &path, const QString &text) {
    QFile file(path);
    if (!file.open(QFile::WriteOnly | QFile::Text | QFile::Truncate)) return false;
    QTextStream stream(&file);
    stream << text;
    return stream.status() == QTextStream::Ok;
}
#endif

QRect centeredCheckboxIndicatorRect(const QStyleOptionViewItem &option, const QWidget *widget) {
    auto *style = widget != nullptr ? widget->style() : QApplication::style();
    QStyleOptionButton checkbox;
    const QSize indicatorSize(
        style->pixelMetric(QStyle::PM_IndicatorWidth, &checkbox, widget),
        style->pixelMetric(QStyle::PM_IndicatorHeight, &checkbox, widget)
    );
    return QStyle::alignedRect(option.direction, Qt::AlignCenter, indicatorSize, option.rect);
}

QColor proxyListSelectedRowColor(const QStyleOptionViewItem &option) {
    const QColor base = option.palette.color(QPalette::Base);
    const bool dark = base.lightness() < 128;
    const int delta = dark ? 34 : -24;
    const int lightness = qBound(0, base.lightness() + delta, 255);
    return QColor::fromHsl(0, 0, lightness);
}

class ProxyListDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        painter->save();

        const auto profileId = index.data(ProxyListModel::ProfileIdRole).toInt();
        const auto *proxyModel = qobject_cast<const ProxyListModel *>(index.model());
        if (proxyModel != nullptr && proxyModel->isProfileSelected(profileId)) {
            painter->fillRect(option.rect, proxyListSelectedRowColor(option));
        }

        if (index.column() == ProxyListModel::ToggleColumn) {
            const auto checkStateData = index.data(Qt::CheckStateRole);
            if (checkStateData.isValid()) {
                auto *style = option.widget != nullptr ? option.widget->style() : QApplication::style();
                QStyleOptionButton checkbox;
                checkbox.rect = centeredCheckboxIndicatorRect(option, option.widget);
                checkbox.palette = option.palette;
                checkbox.state = QStyle::State_Enabled;

                const auto state = static_cast<Qt::CheckState>(checkStateData.toInt());
                if (state == Qt::Checked) {
                    checkbox.state |= QStyle::State_On;
                } else if (state == Qt::PartiallyChecked) {
                    checkbox.state |= QStyle::State_NoChange;
                } else {
                    checkbox.state |= QStyle::State_Off;
                }
                style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &checkbox, painter, option.widget);
            }
            painter->restore();
            return;
        }

        QRect textRect = option.rect.adjusted(6, 0, -6, 0);
        const auto decoration = index.data(Qt::DecorationRole);
        if (decoration.canConvert<QIcon>()) {
            const QSize iconSize(22, 16);
            const QRect iconRect = QStyle::alignedRect(option.direction, Qt::AlignLeft | Qt::AlignVCenter, iconSize, textRect);
            qvariant_cast<QIcon>(decoration).paint(painter, iconRect);
            textRect.setLeft(iconRect.right() + 6);
        }

        const auto foreground = index.data(Qt::ForegroundRole);
        if (foreground.canConvert<QColor>()) {
            painter->setPen(qvariant_cast<QColor>(foreground));
        } else if (foreground.canConvert<QBrush>()) {
            painter->setPen(qvariant_cast<QBrush>(foreground).color());
        } else {
            painter->setPen(option.palette.color(QPalette::Text));
        }

        Qt::Alignment alignment = Qt::AlignVCenter | Qt::AlignLeft;
        const auto alignmentData = index.data(Qt::TextAlignmentRole);
        if (alignmentData.isValid()) alignment = static_cast<Qt::Alignment>(alignmentData.toInt());

        const auto text = index.data(Qt::DisplayRole).toString();
        painter->drawText(textRect, alignment, option.fontMetrics.elidedText(text, Qt::ElideRight, textRect.width()));
        painter->restore();
    }

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override {
        if (index.column() != ProxyListModel::ToggleColumn || !(index.flags() & Qt::ItemIsUserCheckable)) return false;

        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            auto indicatorRect = centeredCheckboxIndicatorRect(option, option.widget);
            if (!indicatorRect.contains(mouseEvent->pos())) return false;
        } else if (event->type() != QEvent::KeyPress) {
            return false;
        }

        auto currentState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        return model->setData(index, currentState == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
    }
};

class SortableTableWidgetItem final : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;

    QTableWidgetItem *clone() const override {
        return new SortableTableWidgetItem(*this);
    }

    bool operator<(const QTableWidgetItem &other) const override {
        const auto lhs = data(Qt::UserRole);
        const auto rhs = other.data(Qt::UserRole);
        if (lhs.isValid() && rhs.isValid()) {
            return lhs.toLongLong() < rhs.toLongLong();
        }
        return QTableWidgetItem::operator<(other);
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

QString groupTabText(const QString &name) {
    constexpr int maxLen = 20;
    if (name.size() <= maxLen) return name;
    return name.left(maxLen) + "...";
}

QList<int> mergeVisibleGroupOrder(const QList<int> &oldOrder, const QList<int> &visibleOrder) {
    QList<int> merged;
    int visibleIndex = 0;
    for (const auto gid: oldOrder) {
        const auto group = ProxorGui::profileManager->GetGroup(gid);
        if (group == nullptr) continue;
        if (group->archive) {
            merged << gid;
        } else if (visibleIndex < visibleOrder.count()) {
            merged << visibleOrder[visibleIndex++];
        }
    }
    while (visibleIndex < visibleOrder.count()) {
        merged << visibleOrder[visibleIndex++];
    }
    return merged;
}

}

void UI_InitMainWindow() {
    mainwindow = new MainWindow;
}

namespace {
// No tray host (GNOME without AppIndicator, Flatpak, bare WM) must never leave an invisible process.
bool noTrayCloseNoticeShown = false;

void ApplyStartupVisibility(MainWindow *w, int waitedMs) {
    using namespace ProxorPlatform;
    const auto tray = CurrentCapability(Capability::SystemTray);
    switch (DecideStartupVisibility(ProxorGui::dataStore->flag_tray, tray, waitedMs, 10000)) {
        case StartupVisibility::StayHidden:
            return;
        case StartupVisibility::WaitForTray:
            QTimer::singleShot(500, w, [w, waitedMs] { ApplyStartupVisibility(w, waitedMs + 500); });
            return;
        case StartupVisibility::ShowWindow:
            w->show();
            if (ProxorGui::dataStore->flag_tray && MW_show_log) MW_show_log(NoTrayStartupNotice(tray));
            return;
    }
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    mainwindow = this;
    MW_dialog_message = [=](const QString &a, const QString &b) {
        runOnUiThread([=] { dialog_message_impl(a, b); });
    };

    // Load Manager
    ProxorGui::profileManager->LoadManager();

    // Test results are per-session: clear persisted latency/report on startup so the table
    // never shows stale ping values from a previous run (possibly a different network).
    for (const auto &[id, profile]: ProxorGui::profileManager->profiles) {
        if (profile == nullptr) continue;
        profile->latency = 0;
        profile->full_test_report.clear();
    }

    // One-time migration: move per-subscription direct sites into the global direct site rules.
    if (!ProxorGui::dataStore->direct_sites_migrated) {
        QJsonArray rules;
        for (const auto &[gid, group]: ProxorGui::profileManager->groups) {
            if (group == nullptr || group->subscription_direct_sites.isEmpty()) continue;
            rules += QJsonObject{
                {"sites", QJsonArray::fromStringList(group->subscription_direct_sites)},
                {"groups", QJsonArray{group->id}},
                {"profiles", QJsonArray{}},
            };
            group->subscription_direct_sites.clear();
            ProxorGui::profileManager->SaveGroup(group);
        }
        if (!rules.isEmpty()) {
            ProxorGui::dataStore->direct_site_rules = QString::fromUtf8(QJsonDocument(rules).toJson(QJsonDocument::Compact));
        }
        ProxorGui::dataStore->direct_sites_migrated = true;
        ProxorGui::dataStore->Save();
    }

    // Setup misc UI
    const auto normalizedTheme = themeManager->NormalizeTheme(ProxorGui::dataStore->theme);
    if (ProxorGui::dataStore->theme != normalizedTheme) {
        ProxorGui::dataStore->theme = normalizedTheme;
        ProxorGui::dataStore->Save();
    }
    ui->setupUi(this);
    themeManager->ApplyTheme(ProxorGui::dataStore->theme);
    for (auto *tabs: {ui->tabWidget, ui->down_tab}) {
        tabs->tabBar()->setUsesScrollButtons(false);
    }
    static const QString toolbarCheckboxSS = QStringLiteral(
        "QCheckBox#checkBox_VPN, QCheckBox#checkBox_SystemProxy {"
        "  padding-top: 0px;"
        "  padding-bottom: 0px;"
        "}"
        "QCheckBox#checkBox_VPN::indicator, QCheckBox#checkBox_SystemProxy::indicator {"
        "  margin-top: 1px;"
        "}");
#ifdef Q_OS_MACOS
    // The System theme on macOS keeps the native checkboxes; the Windows-tuned padding only
    // applies to the other (Fusion/QSS) themes, and is re-evaluated when the theme changes.
    {
        auto applyToolbarCheckboxSS = [this](const QString &themeName) {
            const bool isSystem = (themeManager->NormalizeTheme(themeName) == QStringLiteral("System"));
            ui->checkBox_VPN->setStyleSheet(isSystem ? QString() : toolbarCheckboxSS);
            ui->checkBox_SystemProxy->setStyleSheet(isSystem ? QString() : toolbarCheckboxSS);
        };
        applyToolbarCheckboxSS(ProxorGui::dataStore->theme);
        connect(themeManager, &ThemeManager::themeChanged, this, applyToolbarCheckboxSS);
    }
#else
    ui->checkBox_VPN->setStyleSheet(toolbarCheckboxSS);
    ui->checkBox_SystemProxy->setStyleSheet(toolbarCheckboxSS);
#endif
    m_quotaLabel = new QLabel(this);
    m_quotaLabel->setContentsMargins(0, 0, 8, 0);
    m_quotaLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_quotaLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    ui->down_tab->setCornerWidget(m_quotaLabel, Qt::TopRightCorner);
    m_quotaLabel->hide();
    connect(ui->down_tab, &QTabWidget::currentChanged, this, &MainWindow::on_down_tab_currentChanged);
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        update_connection_statistics_polling_state();
        if (state != Qt::ApplicationActive) {
            application_was_inactive = true;
        } else if (application_was_inactive) {
            application_was_inactive = false;
            queue_resume_subscription_check();
        }
    });
    update_connection_statistics_polling_state();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this](const Qt::ColorScheme &) {
        themeManager->ApplyTheme(ProxorGui::dataStore->theme, true);
    });
#endif
    //
    connect(ui->menu_start, &QAction::triggered, this, [=]() { proxor_start(); });
    connect(ui->menu_stop, &QAction::triggered, this, [=]() { wakeDropRestore(); proxor_stop(); });
    ui->tabWidget->tabBar()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tabWidget->tabBar(), &QTabBar::customContextMenuRequested, this, [=](const QPoint &pos) {
        auto *tabBar = ui->tabWidget->tabBar();
        const int index = tabBar->tabAt(pos);
        if (index < 0) return;

        const int gid = tabBar->tabData(index).toInt();
        if (gid == kAddGroupTabId) return;

        auto group = ProxorGui::profileManager->GetGroup(gid);
        if (group == nullptr) return;

        QMenu menu(this);
        menu.setObjectName("groupTabMenu");
        auto *updateAction = menu.addAction(tr("Update"));
        auto *editAction = menu.addAction(tr("Edit"));
        auto *deleteAction = menu.addAction(tr("Delete"));

        updateAction->setEnabled(!group->url.trimmed().isEmpty());
        deleteAction->setEnabled(ProxorGui::profileManager->groups.size() > 1);

        auto *chosen = menu.exec(tabBar->mapToGlobal(pos));
        if (chosen == updateAction) {
            if (startup_tun_pending || startup_tun_failed) {
                MessageBoxWarning(software_name, tr("Subscription updates are disabled until Tun authorization succeeds."));
                return;
            }
            ProxorGui_sub::groupUpdater->AsyncUpdate(group->url, group->id);
            return;
        }
        if (chosen == editAction) {
            auto dialog = new DialogEditGroup(group, this);
            connect(dialog, &QDialog::finished, this, [=] {
                if (dialog->result() == QDialog::Accepted) {
                    ProxorGui::profileManager->SaveGroup(group);
                    TM_auto_update_subsctiption_Reset_Minute(ProxorGui::dataStore->sub_auto_update);
                    refresh_groups();
                }
                dialog->deleteLater();
            });
            dialog->show();
            return;
        }
        if (chosen == deleteAction) {
            if (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(group->name)) ==
                QMessageBox::StandardButton::Yes) {
                ProxorGui::profileManager->DeleteGroup(group->id);
                refresh_groups();
            }
        }
    });
    connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, [=](int from, int to) {
        Q_UNUSED(from);
        Q_UNUSED(to);
        // use tabData to track tab & gid
        const auto oldOrder = ProxorGui::profileManager->groupsTabOrder;
        QList<int> visibleOrder;
        for (int i = 0; i < ui->tabWidget->tabBar()->count(); i++) {
            auto gid = ui->tabWidget->tabBar()->tabData(i).toInt();
            if (gid == kAddGroupTabId) continue;
            visibleOrder += gid;
        }
        ProxorGui::profileManager->groupsTabOrder = mergeVisibleGroupOrder(oldOrder, visibleOrder);
        ProxorGui::profileManager->SaveManager();
    });
    ui->label_running->installEventFilter(this);
    ui->label_inbound->installEventFilter(this);
    ui->splitter->installEventFilter(this);
    //
    const QStringList hotkeyProblems = RegisterHotkey(false);
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
    // Show menus manually on click so no menu is associated with the button —
    // any associated QMenu causes UxTheme to draw a native drop arrow that
    // cannot be suppressed via QSS on the Windows platform style.
    auto attachMenuOnClick = [](QToolButton *btn, QMenu *menu) {
#ifdef Q_OS_MACOS
        btn->setFocusPolicy(Qt::NoFocus);
        // Pop the menu up as a native NSMenu, like the menu-bar menus (the Qt-drawn popup had square
        // corners, no shortcuts, and on the Settings button did not show at all, leaving it pressed).
        QObject::connect(btn, &QToolButton::clicked, btn, [btn, menu]() {
            ProxorMac::PopupMenu(menu, btn);
            btn->setDown(false);
            btn->update();
        });
#else
        // Keyboard users have no menu bar here: let Tab reach the button, Space/Enter opens the menu.
        btn->setFocusPolicy(Qt::TabFocus);
        btn->installEventFilter(new EnterClicksFilter(btn));
        QObject::connect(btn, &QToolButton::clicked, btn, [btn, menu]() {
            menu->popup(btn->mapToGlobal(QPoint(0, btn->height())));
        });
#endif
        QObject::connect(menu, &QMenu::aboutToHide, btn, [btn]() {
            btn->setDown(false);
            btn->update();
        });
    };
    attachMenuOnClick(ui->toolButton_program, ui->menu_program);
    attachMenuOnClick(ui->toolButton_preferences, ui->menu_preferences);
    attachMenuOnClick(ui->toolButton_server, ui->menu_server);
    ui->menubar->setVisible(false);
#ifdef Q_OS_MACOS
    // QMenuBar::setVisible(false) does not remove a native menu bar, so menu_program /
    // menu_preferences / menu_server stay in the macOS global menu bar. Qt's
    // TextHeuristicRole would then move any action whose caption starts with
    // About/Settings/Preferences/Quit/etc. into the application menu (Qt 6.11
    // qcocoamenuitem.mm detectMenuRole), emptying Exit / Settings / About Proxor out of
    // Proxor's own menus (and the mirrored tray menu built below, same QAction objects).
    // NoRole keeps every one of these actions exactly where the .ui put them.
    {
        std::function<void(QAction *)> forceNoRole = [&](QAction *action) {
            action->setMenuRole(QAction::NoRole);
            if (auto *sub = action->menu()) {
                for (auto *subAction : sub->actions()) forceNoRole(subAction);
            }
        };
        for (auto *topMenu : {ui->menu_program, ui->menu_preferences, ui->menu_server}) {
            for (auto *action : topMenu->actions()) forceNoRole(action);
        }
    }
    // Mac convention: a "Proxor" application-menu entry for Settings... (Cmd+,) and About
    // Proxor, wired to the same slots as the .ui actions above. These are separate QActions
    // that exist only here, so menu_basic_settings/menu_about stay NoRole and visible in
    // menu_preferences/menu_program. No QuitRole action is added here: Qt's default
    // "Quit Proxor" item (Cmd+Q) is handled by the spontaneous-Quit interceptor installed
    // further below, which runs Proxor's real exit path.
    {
        auto *macAppRoleMenu = ui->menubar->addMenu(QStringLiteral("Proxor"));
        auto *macSettingsAction = macAppRoleMenu->addAction(tr("Settings…"));
        macSettingsAction->setMenuRole(QAction::PreferencesRole);
        macSettingsAction->setShortcut(QKeySequence::Preferences);
        connect(macSettingsAction, &QAction::triggered, this, &MainWindow::on_menu_basic_settings_triggered);
        auto *macAboutAction = macAppRoleMenu->addAction(tr("About Proxor"));
        macAboutAction->setMenuRole(QAction::AboutRole);
        connect(macAboutAction, &QAction::triggered, this, [this] { ui->menu_about->trigger(); });
    }
#endif
    auto applyToolbarAutoRaise = [this](const QString &themeName) {
        const bool isSystem = (themeManager->NormalizeTheme(themeName) == QStringLiteral("System"));
        const QList<QToolButton *> btns = {
            ui->toolButton_toggle_proxy,
            ui->toolButton_program,
            ui->toolButton_preferences,
            ui->toolButton_server,
            ui->toolButton_url_test,
            ui->toolButton_update_subscription,
        };
        static const QString systemBtnSS = QStringLiteral(
            "QToolButton {"
            "  background-color: palette(button);"
            "  border: 1px solid palette(mid);"
            "  border-radius: 6px;"
            "}"
            "QToolButton:hover, QToolButton:open {"
            "  background-color: palette(light);"
            "  border-color: palette(shadow);"
            "}");
#ifdef Q_OS_MACOS
        // QMacStyle draws a QToolButton outside a QToolBar as a boxed bevel, which gives the same
        // boxed buttons as the other platforms in the Mac style. The Windows-tuned systemBtnSS
        // would replace that native drawing, so every button keeps no style sheet here.
        (void) systemBtnSS;
        (void) isSystem;
        for (auto *btn : btns) {
            btn->setAutoRaise(false);
            btn->setStyleSheet(QString());
        }
#else
        for (auto *btn : btns) {
            btn->setAutoRaise(isSystem);
            btn->setStyleSheet(isSystem ? systemBtnSS : QString());
        }
#endif
    };
    applyToolbarAutoRaise(ProxorGui::dataStore->theme);
    connect(themeManager, &ThemeManager::themeChanged, this, applyToolbarAutoRaise);
    ui->toolButton_toggle_proxy->setText(tr("Start"));
    ui->toolButton_toggle_proxy->setIcon(makeToggleProxyIcon(QColor(52, 199, 89)));
    ui->toolButton_toggle_proxy->setIconSize(QSize(24, 24));
    ui->toolButton_toggle_proxy->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    setTimeout([this] {
        const int referenceHeight = qMax(ui->toolButton_program->height(), ui->toolButton_program->sizeHint().height());
        const auto toolbarButtons = {
            ui->toolButton_toggle_proxy,
            ui->toolButton_program,
            ui->toolButton_preferences,
            ui->toolButton_server,
        };
        for (auto *button: toolbarButtons) {
            button->setIconSize(QSize(24, 24));
            button->setMinimumHeight(referenceHeight);
            button->setMinimumWidth(referenceHeight);
        }
#ifndef Q_OS_MACOS
        const int stackedSpacing = ui->verticalLayout_url_sub->spacing();
        const int stackedAvailableHeight = qMax(2, referenceHeight - stackedSpacing);
        const int topButtonHeight = stackedAvailableHeight / 2;
        const int bottomButtonHeight = stackedAvailableHeight - topButtonHeight;
        ui->toolButton_url_test->setMinimumHeight(topButtonHeight);
        ui->toolButton_url_test->setMaximumHeight(topButtonHeight);
        ui->toolButton_update_subscription->setMinimumHeight(bottomButtonHeight);
        ui->toolButton_update_subscription->setMaximumHeight(bottomButtonHeight);
        ui->toolButton_url_test->setMinimumWidth(ui->toolButton_update_subscription->sizeHint().width());

        const int checkboxSpacing = ui->verticalLayout_4->spacing();
        const int checkboxAvailableHeight = qMax(2, referenceHeight - checkboxSpacing);
        const int topCheckboxHeight = checkboxAvailableHeight / 2;
        const int bottomCheckboxHeight = checkboxAvailableHeight - topCheckboxHeight;
        ui->checkBox_VPN->setMinimumHeight(topCheckboxHeight);
        ui->checkBox_VPN->setMaximumHeight(topCheckboxHeight);
        ui->checkBox_SystemProxy->setMinimumHeight(bottomCheckboxHeight);
        ui->checkBox_SystemProxy->setMaximumHeight(bottomCheckboxHeight);
#endif
    }, this, 0);
#ifdef Q_OS_MACOS
    {
        // The Windows-tuned forced heights squeeze the native Mac bevels (the stacked buttons
        // overlapped and the check boxes sat above their labels), so on macOS the native size
        // hints win under every theme.
        auto applyStackedHeights = [this](const QString &) {
            for (QWidget *w : {static_cast<QWidget *>(ui->toolButton_url_test),
                               static_cast<QWidget *>(ui->toolButton_update_subscription),
                               static_cast<QWidget *>(ui->checkBox_VPN),
                               static_cast<QWidget *>(ui->checkBox_SystemProxy)}) {
                w->setMinimumHeight(0);
                w->setMaximumHeight(QWIDGETSIZE_MAX);
            }
        };
        setTimeout([this, applyStackedHeights] { applyStackedHeights(ProxorGui::dataStore->theme); }, this, 0);
        connect(themeManager, &ThemeManager::themeChanged, this, applyStackedHeights);
    }
#endif
    connect(ui->toolButton_url_test, &QToolButton::clicked, this, [=] {
        const int m = ProxorGui::dataStore->ping_type == 1 ? 3 : (ProxorGui::dataStore->ping_type == 2 ? 4 : 0);
        speedtest_current_group(m, true);
    });
    connect(ui->toolButton_update_subscription, &QToolButton::clicked, this, [=] { on_menu_update_subscription_triggered(); });

    // Setup log UI
    ui->splitter->restoreState(DecodeB64IfValid(ProxorGui::dataStore->splitter_state));
    qvLogDocument->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setDocument(qvLogDocument);
    auto bottomPaneFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
#ifdef Q_OS_MACOS
    bottomPaneFont.setPointSize(11); // macOS points are 72-dpi based, 9 pt is too small there
#else
    // Respect the system font size instead of forcing 9 pt (high-DPI and accessibility font settings).
    const qreal systemPointSize = QFontDatabase::systemFont(QFontDatabase::GeneralFont).pointSizeF();
    if (systemPointSize > 0) bottomPaneFont.setPointSizeF(systemPointSize);
#endif
    ui->masterLogBrowser->setFont(bottomPaneFont);
    qvLogDocument->setDefaultFont(bottomPaneFont);
    // Keep log and connection tabs visually aligned.
    ui->log_filter->setFont(bottomPaneFont);
    ui->conn_filter->setFont(bottomPaneFont);
    ui->tableWidget_conn->setFont(bottomPaneFont);
    ui->tableWidget_conn->horizontalHeader()->setFont(bottomPaneFont);
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
    // Every line also goes to disk. These three assignments are the only sink, so the 45
    // MW_show_log call sites needed no change.
    MW_show_log = [=](const QString &log) {
        ProxorGui_log::Write(ProxorGui_log::InferLevel(log), log);
        runOnUiThread([=] { show_log_impl(log); });
    };
    MW_show_log_ext = [=](const QString &tag, const QString &log) {
        const QString line = "[" + tag + "] " + log;
        ProxorGui_log::Write(ProxorGui_log::InferLevel(log), line);
        runOnUiThread([=] { show_log_impl(line); });
    };
    MW_show_log_ext_vt100 = [=](const QString &log) {
        ProxorGui_log::Write(ProxorGui_log::InferLevel(log), log);
        runOnUiThread([=] { show_log_impl(log); });
    };
    // Diagnostic only: goes to the log file, not the window log.
    ProxorGui_log::Write(ProxorGui_log::Level::Info, "Platform: " + ProxorPlatform::DescribePlatformEnvironment(ProxorPlatform::CurrentPlatformEnvironment()));
    for (const auto &problem : hotkeyProblems) MW_show_log(tr("Hotkeys: %1").arg(problem));

    // table UI
    proxyListModel = new ProxyListModel(this);
    ui->proxyListTable->setModel(proxyListModel);
    ui->proxyListTable->setItemDelegate(new ProxyListDelegate(ui->proxyListTable));
    auto refreshTableTheme = [this](const QString &) {
        for (auto *view : {static_cast<QAbstractItemView *>(ui->proxyListTable),
                           static_cast<QAbstractItemView *>(ui->tableWidget_conn)}) {
            view->setPalette(QPalette());
            view->setStyleSheet(QString());
            view->style()->unpolish(view);
            view->style()->polish(view);
            view->viewport()->setPalette(QPalette());
            view->viewport()->style()->unpolish(view->viewport());
            view->viewport()->style()->polish(view->viewport());
            view->viewport()->update();
        }
        for (auto *header : {ui->proxyListTable->horizontalHeader(),
                             ui->proxyListTable->verticalHeader(),
                             ui->tableWidget_conn->horizontalHeader(),
                             ui->tableWidget_conn->verticalHeader()}) {
            header->setPalette(QPalette());
            header->setStyleSheet(QString());
            header->style()->unpolish(header);
            header->style()->polish(header);
            header->update();
        }
        ui->proxyListTable->setAlternatingRowColors(false);
        ui->tableWidget_conn->setAlternatingRowColors(true);
    };
    refreshTableTheme(ProxorGui::dataStore->theme);
    connect(themeManager, &ThemeManager::themeChanged, this, refreshTableTheme);
#ifdef Q_OS_MACOS
    {
        // System on macOS: native tab panes (same framed layout as the other platforms) and
        // native alternating rows without grid lines. Other themes get today's values back.
        // Connected after refreshTableTheme, so it runs after that lambda resets the palettes
        // on every themeChanged.
        auto applyMacNativeLook = [this](const QString &themeName) {
            const bool isSystem = (themeManager->NormalizeTheme(themeName) == QStringLiteral("System"));
            ui->tabWidget->setDocumentMode(false);
            ui->down_tab->setDocumentMode(false);
            ui->proxyListTable->setAlternatingRowColors(isSystem);
            ui->tableWidget_conn->setAlternatingRowColors(true);
            ui->tableWidget_conn->setShowGrid(!isSystem);
        };
        applyMacNativeLook(ProxorGui::dataStore->theme);
        connect(themeManager, &ThemeManager::themeChanged, this, applyMacNativeLook);
    }
#endif
    connect(themeManager, &ThemeManager::themeChanged, this, [=](const QString &) {
        rebuildLogDocument(ui->log_filter->text());
    });
    ui->proxyListTable->verticalHeader()->setVisible(false);
    ui->proxyListTable->setShowGrid(false);
    ui->proxyListTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    ui->proxyListTable->horizontalHeader()->setStretchLastSection(false);
    auto *filterHeader = new ProxyListFilterHeader(ui->proxyListTable->horizontalHeader());
    connect(filterHeader, &ProxyListFilterHeader::filterChanged, ui->proxyListTable, &ProxyListView::setColumnFilter);
    auto *btnFilter = new QToolButton(this);
    btnFilter->setIcon(QIcon(QStringLiteral(":/icon/filter.png")));
    btnFilter->setToolTip(QString("%1\n%2").arg(tr("Enable Filter"),
        QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText)));
    btnFilter->setShortcut(QKeySequence::Find);
    btnFilter->setCheckable(true);
    connect(btnFilter, &QToolButton::toggled, filterHeader, &ProxyListFilterHeader::setFiltersVisible);
    ui->tabWidget->setCornerWidget(btnFilter, Qt::TopRightCorner);
    connect(proxyListModel, &ProxyListModel::orderChanged, this, [=](const QList<int> &order) {
        auto group = ProxorGui::profileManager->CurrentGroup();
        if (group == nullptr) return;
        group->order = order;
        ProxorGui::profileManager->SaveGroup(group);
    });
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
        static const int colMinWidths[] = {0, 75, 60, 90, 60, 120};
        static const int colMaxWidths[] = {0, 600, 200, 600, 200, 400};
        int minW = (logicalIndex < 6) ? colMinWidths[logicalIndex] : 0;
        int maxW = (logicalIndex < 6) ? colMaxWidths[logicalIndex] : 0;
        if (minW > 0 && newSize < minW) {
            auto header = ui->proxyListTable->horizontalHeader();
            header->blockSignals(true);
            header->resizeSection(logicalIndex, minW);
            header->blockSignals(false);
            newSize = minW;
        } else if (maxW > 0 && newSize > maxW) {
            auto header = ui->proxyListTable->horizontalHeader();
            header->blockSignals(true);
            header->resizeSection(logicalIndex, maxW);
            header->blockSignals(false);
            newSize = maxW;
        }
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
    ui->search->setPlaceholderText(tr("Search profiles"));
    // Accessible names for screen readers (no visible change).
    ui->search->setAccessibleName(tr("Search profiles"));
    ui->proxyListTable->setAccessibleName(tr("Profiles"));
    ui->masterLogBrowser->setAccessibleName(tr("Log"));
    ui->tableWidget_conn->setAccessibleName(tr("Connections"));
    ui->log_filter->setAccessibleName(tr("Filter log"));
    ui->conn_filter->setAccessibleName(tr("Filter connections"));
    ui->label_running->setAccessibleName(tr("Running profile"));
    ui->search->setMinimumWidth(120);
    ui->search->setVisible(false);
    auto showSearch = [=] {
        ui->search->setVisible(true);
        ui->search->setFocus();
    };
    // Same pattern as Ctrl+V: the menu action shows the shortcut, the QShortcut covers the hidden menu bar.
    connect(shortcut_ctrl_f, &QShortcut::activated, this, showSearch);
    connect(ui->menu_find, &QAction::triggered, this, showSearch);
    connect(shortcut_ctrl_v, &QShortcut::activated, this, [=] {
        on_menu_add_from_clipboard_triggered();
    });
    connect(shortcut_ctrl_a, &QShortcut::activated, this, [=] {
        ui->menu_select_all->trigger();
    });
    connect(shortcut_ctrl_c, &QShortcut::activated, this, [=] {
        ui->menu_copy_links->trigger();
    });
    connect(shortcut_ctrl_s, &QShortcut::activated, this, [=] {
        ui->menu_stop->trigger();
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
    connect(ui->search, &QLineEdit::textChanged, this, &MainWindow::apply_proxy_list_search);

    // refresh
    this->refresh_groups();

    // Setup Tray
    tray = new QSystemTrayIcon(this); // 初始化托盘对象tray
    // Tray context menu: only what is useful from the tray. The full App menu stays in the toolbar.
    auto buildTrayMenu = [this]() {
        const QList<QAction *> hidden{ui->menu_spmode->menuAction(), ui->menu_check_updates, ui->menu_about,
                                      ui->menu_report_bug, ui->menu_donate, ui->actionRestart_Program};
        auto *menu = new QMenu(this);
        bool pendingSeparator = false;
        for (auto *action : ui->menu_program->actions()) {
            const bool isRestartProxy = action == ui->actionRestart_Proxy; // replaced by Start / Stop
            if (!isRestartProxy && hidden.contains(action)) continue;
            if (action->isSeparator()) {
                pendingSeparator = !menu->isEmpty();
                continue;
            }
            if (pendingSeparator) menu->addSeparator();
            pendingSeparator = false;
#ifdef Q_OS_MACOS
            if (isRestartProxy) {
                // Same switch as Settings > "Show speed in the menu bar"; kept in sync when the menu opens.
                auto *speedAction = new QAction(tr("Show speed in the menu bar"), menu);
                speedAction->setCheckable(true);
                speedAction->setChecked(ProxorGui::dataStore->tray_speed_view);
                connect(speedAction, &QAction::toggled, this, [this](bool checked) {
                    if (ProxorGui::dataStore->tray_speed_view == checked) return;
                    ProxorGui::dataStore->tray_speed_view = checked;
                    ProxorGui::dataStore->Save();
                    if (!checked) update_tray_speed(0, 0, true);
                });
                connect(menu, &QMenu::aboutToShow, speedAction, [speedAction] {
                    QSignalBlocker blocker(speedAction);
                    speedAction->setChecked(ProxorGui::dataStore->tray_speed_view);
                });
                menu->addAction(speedAction);
                menu->addSeparator();
            }
#endif
            if (isRestartProxy) {
                // One action for both directions, same behavior as the Start/Stop button of the window.
                tray_toggle_action = new QAction(tr("Start"), menu);
                connect(tray_toggle_action, &QAction::triggered, this, [this] { on_toolButton_toggle_proxy_clicked(); });
                menu->addAction(tray_toggle_action);
                continue;
            }
            menu->addAction(action);
        }
        return menu;
    };

#ifdef Q_OS_MACOS
    // Never call tray->setContextMenu/setIcon/show() on macOS: QSystemTrayIcon::setContextMenu
    // is exactly what crashes (see MacPlatform.mm's header comment and tray-crash.log). `tray`
    // stays around only so actionShow_window and the hotkey below can keep emitting
    // tray->activated(Trigger) into the same lambda; its own (never shown) status item is inert.
    // Proxor owns a real NSStatusItem instead, with a dedicated tray menu (not shared with the
    // toolbar's ui->menu_program, since one NSMenu cannot have two supermenus).
    mac_tray_menu = buildTrayMenu();
    mac_status_item = new ProxorMac::StatusItem;
    mac_status_item->setColored(ProxorGui::dataStore->tray_icon_colored);
    mac_status_item->setActive(false);
    mac_status_item->setIcon(Icon::GetTrayIcon(Icon::NONE));
    mac_status_item->setMenu(mac_tray_menu);
    mac_status_item->setVisible(true);
    // Started hidden in the menu bar (-tray): no Dock icon until the window is shown.
    QTimer::singleShot(0, this, [this] {
        if (!isVisible() && !isMinimized()) ProxorMac::SetDockIconVisible(false);
    });
    ProxorMac::InstallQuitInterceptor(this, [this] {
        if (!ProxorGui::dataStore->prepare_exit) on_menu_exit_triggered();
    });
    // Dock icon click (kAEReopenApplication): bring the hidden or minimized window back. Never fires at launch.
    ProxorMac::InstallReopenHandler(this, [this] {
        if (ProxorPlatform::DecideReopen(isVisible(), isMinimized(), ProxorGui::dataStore->prepare_exit) != ProxorPlatform::ReopenAction::ShowWindow) return;
        ProxorGui_log::Write(ProxorGui_log::Level::Info, ProxorPlatform::ReopenLogLine());
        ActivateWindow(this);
    });
    // Result of an in-place update that ran while Proxor was closed (phase 60).
    QTimer::singleShot(0, this, [this] { macAppUpdateShowResult(); });
#else
    tray->setIcon(Icon::GetTrayIcon(Icon::NONE));
    tray->setContextMenu(buildTrayMenu()); // dedicated tray menu (not the toolbar App menu)
    tray->show();                           // 让托盘图标显示在系统托盘上
#endif
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
    connect(ui->menu_check_updates, &QAction::triggered, this, [=] { runOnNewThread([=] { CheckUpdate(); }); });
    connect(ui->menu_report_bug, &QAction::triggered, this, [] { QDesktopServices::openUrl(bugReportUrl()); });
    connect(ui->menu_donate, &QAction::triggered, this, [] { QDesktopServices::openUrl(sponsorUrl()); });
    connect(ui->actionRestart_Proxy, &QAction::triggered, this, [=] { if (ProxorGui::dataStore->started_id>=0) proxor_start(ProxorGui::dataStore->started_id); });
    connect(ui->actionRestart_Program, &QAction::triggered, this, [=] { MW_dialog_message("", "RestartProgram"); });
    connect(ui->actionShow_window, &QAction::triggered, this, [=] { tray->activated(QSystemTrayIcon::ActivationReason::Trigger); });
    //
    connect(ui->checkBox_VPN, &QCheckBox::clicked, this, [=](bool checked) { proxor_set_spmode_vpn(checked); });
#ifdef Q_OS_MACOS
    connect(MacHelperSvc(), &MacHelperService::tunReady, this, &MainWindow::macOnTunReady);
    connect(MacHelperSvc(), &MacHelperService::tunStopped, this, &MainWindow::macOnTunStopped);
    connect(MacHelperSvc(), &MacHelperService::helperLog, this, [=](const QString &line) { MW_show_log("[Tun] " + line); });
    connect(MacHelperSvc(), &MacHelperService::connectionLost, this, &MainWindow::macOnHelperLost);
    mac_modes = new MacModeCoordinator({
        [this] { return ProxorGui::dataStore->spmode_system_proxy; },
        [this] { return ProxorGui::dataStore->spmode_vpn; },
        [this] { return vpn_pid != 0; },
        [this] { return ProxorGui::dataStore->started_id >= 0; },
        [this](bool sp, bool tun) { macPauseModes(sp, tun); },
        [this](bool sp, bool tun) { macResumeModes(sp, tun); },
    }, kMacUserStopGraceMs, this);
#endif
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
#ifndef NKR_CAMERA_SCAN
    ui->menu_scan_qr_camera->setVisible(false); // this build has no camera support
#endif
#ifdef NKR_NO_ZXING
    // on_menu_scan_qr_triggered needs the barcode reader this build does not link.
    ui->menu_scan_qr->setVisible(false);
    ui->menu_scan_qr_image->setVisible(false);
    ui->menu_scan_qr_clipboard->setVisible(false);
#endif
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
            QList<std::shared_ptr<ProxorGui::ProxyEntity>> profiles;
            if (menuCurrent_Select) {
                profiles = get_now_selected_list();
            } else {
                auto group = ProxorGui::profileManager->CurrentGroup();
                if (group != nullptr) {
                    profiles = group->ProfilesWithOrder();
                }
            }
            bool canResolveDomain = false;
            for (const auto &profile: profiles) {
                if (profile != nullptr && profile->bean->CanResolveDomainToIP()) {
                    canResolveDomain = true;
                    break;
                }
            }
            ui->menu_resolve_domain->setEnabled(canResolveDomain);
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

    // Set up the gRPC client synchronously, before anything can use it.
    // This only needs core_port/core_token (assigned above) — the Client
    // constructor opens no connection, the channel is built per Call().
    // Doing it inside the DS_cores lambda below raced against the startup
    // update check, which runs off its own UI-thread timer and dereferenced
    // a still-null defaultClient.
    setup_grpc();

    const bool restore_system_proxy = ProxorGui::dataStore->remember_spmode.contains("system_proxy");
#ifdef Q_OS_MACOS
    // A remembered Tun holds the remembered profile only until the asynchronous helper probe answers
    // (macStartupProbed, bounded by a safety timer): Ready restores it, any other state starts the profile
    // without Tun (MacHelperPolicy). Fresh configs land here too: DataStore defaults remember_spmode to {"vpn"}.
    const bool mac_remembered_vpn = ProxorGui::dataStore->remember_spmode.contains("vpn") || ProxorGui::dataStore->flag_restart_tun_on;
    mac_startup_probe_pending = mac_remembered_vpn || restore_system_proxy;
    const bool restore_vpn = mac_remembered_vpn; // provisional until the probe answers
#else
    const bool restore_vpn = ProxorGui::dataStore->remember_spmode.contains("vpn") || ProxorGui::dataStore->flag_restart_tun_on;
#endif
    // A remembered TUN must be ready before any automatic work creates traffic.
    startup_tun_pending = restore_vpn;
    if (startup_tun_pending && ProxorGui::dataStore->remember_enable && ProxorGui::dataStore->remember_id >= 0) {
        startup_deferred_profile_id = ProxorGui::dataStore->remember_id;
    }

    // Start core
    runOnUiThread(
        [=] {
            core_process = new ProxorGui_sys::CoreProcess(core_path, args);
            // Remember last started
            if (ProxorGui::dataStore->remember_enable && ProxorGui::dataStore->remember_id >= 0) {
                if (startup_tun_pending && !startup_tun_authorized) {
                    startup_deferred_profile_id = ProxorGui::dataStore->remember_id;
                } else {
                    core_process->start_profile_when_core_is_up = ProxorGui::dataStore->remember_id;
                }
            }
            // Setup
            core_process->Start();
        },
        DS_cores);

    wifi_monitor = new WifiMonitor(CreatePlatformWifiBackend(), 5000, this);
    WifiMonitor::setAppInstance(wifi_monitor);
    connect(wifi_monitor, &WifiMonitor::ssidChanged, this, &MainWindow::onWifiSsidChanged);
    connect(wifi_monitor, &WifiMonitor::readingChanged, this, &MainWindow::onWifiReadingChanged);
    refreshWifiMonitoring();
#ifdef Q_OS_MACOS
    if (!ProxorGui::dataStore->local_network_prompted) {
        ProxorGui::dataStore->local_network_prompted = true;
        ProxorGui::dataStore->Save();
        // After the Location prompt, so the two native prompts do not start together.
        QTimer::singleShot(2500, this, [] { MacLocalNetwork::TriggerPrompt(); });
    }
#endif

    connect(qApp, &QGuiApplication::commitDataRequest, this, &MainWindow::on_commitDataRequest);

    auto t = new QTimer;
    connect(t, &QTimer::timeout, this, [=]() { refresh_status(); });
    t->start(2000);

    t = new QTimer;
    connect(t, &QTimer::timeout, this, [&] { ProxorGui_sys::logCounter.fetchAndStoreRelaxed(0); });
    t->start(1000);

    TM_auto_update_subsctiption = new QTimer;
    TM_auto_update_subsctiption_Reset_Minute = [&](int) {
        TM_auto_update_subsctiption->stop();
        if (UI_has_scheduled_subscription_updates()) {
            TM_auto_update_subsctiption->start(60 * 1000);
        }
    };
    connect(TM_auto_update_subsctiption, &QTimer::timeout, this, [this] {
        const auto now = QDateTime::currentMSecsSinceEpoch();
        const bool resumed = subscription_timer_last_tick_ms > 0 && now - subscription_timer_last_tick_ms > 70 * 1000;
        subscription_timer_last_tick_ms = now;
        if (resumed) {
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
            wakeDetected(ProxorPlatform::WakeSource::TimerGap);
#else
            queue_resume_subscription_check();
#endif
            return;
        }
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
        if (wakeOwnsSubscriptions()) return; // the wake run updates subscriptions until it finishes
#endif
        if (!startup_tun_pending && !startup_tun_failed) UI_update_due_groups_on_timer();
    });
    TM_auto_update_subsctiption_Reset_Minute(ProxorGui::dataStore->sub_auto_update);
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    wakeInstall();
#endif
    #if QT_VERSION >= QT_VERSION_CHECK(6, 4, 0)
    const bool niLoaded = QNetworkInformation::loadBackendByFeatures(QNetworkInformation::Feature::Reachability);
    #elif QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    const bool niLoaded = QNetworkInformation::loadDefaultBackend();
    #else
    const bool niLoaded = false;
    #endif
    QNetworkInformation *ni = niLoaded ? QNetworkInformation::instance() : nullptr;
    const bool isOnline = ni && ni->reachability() == QNetworkInformation::Reachability::Online;

    startup_network_work = [this, ni, isOnline] {
        if (ProxorGui::dataStore->sub_update_on_start) {
            if (!ni || isOnline) {
                setTimeout([this] { UI_update_all_groups(true); }, this, 2000);
            } else {
                runOnceWhenOnline(ni, this, [this] { UI_update_all_groups(true); });
            }
        } else {
            if (!ni || isOnline) {
                setTimeout([this] { UI_update_due_groups_on_start(); }, this, 2000);
            } else {
                runOnceWhenOnline(ni, this, [this] { UI_update_due_groups_on_start(); });
            }
        }
        setTimeout([this] { run_subscription_ping_on_open(); }, this, 2500);

        if (ProxorGui::dataStore->check_update_on_start) {
            auto doCheck = [this]() {
                setTimeout([this] { runOnNewThread([this] { CheckUpdate(true); }); }, this, 1500);
            };
            if (!ni || isOnline) {
                doCheck();
            } else {
                runOnceWhenOnline(ni, this, [this] { runOnNewThread([this] { CheckUpdate(true); }); });
            }
        }
    };
    if (!startup_tun_pending) {
        auto startupWork = std::move(startup_network_work);
        startupWork();
    }

#ifdef Q_OS_MACOS
    ProxorMac::InstallDialogPolish();
    ProxorMac::PolishMainWindow(this);
#endif
    ApplyStartupVisibility(this, 0);

    // Restore spmode after the window has entered the event loop so prompts
    // like the Tun admin warning are shown the same way as manual activation.
    if (ProxorGui::dataStore->remember_enable || ProxorGui::dataStore->flag_restart_tun_on || restore_vpn) {
        setTimeout([=] {
#ifdef Q_OS_MACOS
            macStartupRestore(restore_system_proxy, restore_vpn);
#else
            if (restore_system_proxy) {
                proxor_set_spmode_system_proxy(true, false);
            }
            if (restore_vpn) {
                proxor_set_spmode_vpn(true, false);
                if (ProxorGui::UseInternalTun() && ProxorGui::dataStore->spmode_vpn) {
                    completeStartupTunAuthorization();
                } else if (!ProxorGui::UseInternalTun() && !ProxorGui::dataStore->spmode_vpn) {
                    failStartupTunAuthorization();
                }
            }
#endif
        }, this, 0);
    }
}

void MainWindow::run_subscription_ping_on_open(int attempts) {
    if (startup_tun_pending || startup_tun_failed) return;
    constexpr int maxAttempts = 90;
    if (attempts > maxAttempts) return;

    // Tun carries the tests, and the core installs its routes after it answers on gRPC, so
    // a started profile and a settled tunnel are both required. Without this the list came
    // back entirely unavailable on every platform, because the probes left before the
    // tunnel could carry them.
    const bool tunSelected = ProxorGui::dataStore->spmode_vpn;
    const bool tunSettled = g_tun_enabled_ms > 0 &&
                            QDateTime::currentMSecsSinceEpoch() - g_tun_enabled_ms >= kTunSettleMs;
    const bool waitingForTun = tunSelected && (ProxorGui::dataStore->started_id < 0 || !tunSettled);

    if (UI_subscription_updates_running() || !ProxorGui::dataStore->core_running || waitingForTun) {
        setTimeout([this, attempts] { run_subscription_ping_on_open(attempts + 1); }, this, 1000);
        return;
    }

    QList<std::shared_ptr<ProxorGui::ProxyEntity>> profiles;
    QSet<int> profileIds;
    for (const auto gid: ProxorGui::profileManager->groupsTabOrder) {
        const auto group = ProxorGui::profileManager->GetGroup(gid);
        if (group == nullptr || group->archive || group->skip_auto_update || group->url.isEmpty()) continue;
        if (!group->subscription_ping_onopen_enabled) continue;

        for (const auto &profile: group->ProfilesWithOrder()) {
            if (profile == nullptr || profileIds.contains(profile->id)) continue;
            profileIds += profile->id;
            profiles += profile;
        }
    }

    if (profiles.isEmpty()) return;

    speedtest_profiles(profiles, 3, true, true, true); // ICMP, silent
}

void MainWindow::closeEvent(QCloseEvent *event) {
#ifdef Q_OS_MACOS
    if (mac_status_item != nullptr && mac_status_item->isVisible()) {
#else
    if (tray->isVisible()) {
#endif
        const auto trayCap = ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::SystemTray);
        if (ProxorPlatform::DecideCloseAction(trayCap) == ProxorPlatform::CloseAction::Minimize) {
            showMinimized();
            event->ignore();
            if (!noTrayCloseNoticeShown) {
                noTrayCloseNoticeShown = true;
                MW_show_log(ProxorPlatform::NoTrayCloseNotice(trayCap));
            }
            return;
        }
        ui->proxyListTable->clearSelection();
        hide();          // 隐藏窗口
        event->ignore(); // 忽略事件
    }
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange || event->type() == QEvent::ActivationChange) {
        update_connection_statistics_polling_state();
    }
    if (event->type() == QEvent::ApplicationPaletteChange && !themeManager->applying) {
        themeManager->ApplyTheme(ProxorGui::dataStore->theme, true);
    }
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    update_connection_statistics_polling_state();
#ifdef Q_OS_MACOS
    ProxorMac::SetDockIconVisible(true);
#endif
#ifdef Q_OS_WIN
    themeManager->ReapplyTitleBar();
#endif
}

void MainWindow::hideEvent(QHideEvent *event) {
    QMainWindow::hideEvent(event);
    update_connection_statistics_polling_state();
#ifdef Q_OS_MACOS
    // Closed to the menu bar: no Dock icon. A minimized window keeps it, since it lives in the Dock.
    if (!isMinimized() && mac_status_item != nullptr && mac_status_item->isVisible()) ProxorMac::SetDockIconVisible(false);
#endif
}

MainWindow::~MainWindow() {
    delete ui;
}

std::shared_ptr<ProxorGui::ProxyEntity> MainWindow::resolveSsidOnDemandProfile() const {
    auto *dataStore = ProxorGui::dataStore;
    auto syncStoredProfileRef = [dataStore](const std::shared_ptr<ProxorGui::ProxyEntity> &profile) {
        if (profile == nullptr) return false;

        const bool changed =
            dataStore->ssid_on_demand_profile_id != profile->id ||
            dataStore->ssid_on_demand_profile_name != profile->summary_name ||
            dataStore->ssid_on_demand_profile_type != profile->type ||
            dataStore->ssid_on_demand_profile_address.compare(profile->summary_serverAddress, Qt::CaseInsensitive) != 0 ||
            dataStore->ssid_on_demand_profile_port != profile->summary_serverPort;
        if (!changed) return false;

        dataStore->ssid_on_demand_profile_id = profile->id;
        dataStore->ssid_on_demand_profile_name = profile->summary_name;
        dataStore->ssid_on_demand_profile_type = profile->type;
        dataStore->ssid_on_demand_profile_address = profile->summary_serverAddress;
        dataStore->ssid_on_demand_profile_port = profile->summary_serverPort;
        dataStore->Save();
        return true;
    };

    auto direct = ProxorGui::profileManager->GetProfile(dataStore->ssid_on_demand_profile_id);
    if (direct != nullptr) {
        syncStoredProfileRef(direct);
        return direct;
    }

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
    for (const auto &[_, profile] : ProxorGui::profileManager->profiles) {
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

    syncStoredProfileRef(best.profile);
    return best.profile;
}

void MainWindow::refreshWifiMonitoring() {
    if (wifi_monitor == nullptr || ProxorGui::dataStore == nullptr) return;
    const QString hosts = ProxorGui::dataStore->routing != nullptr ? ProxorGui::dataStore->routing->hosts_mapping : QString();
    const bool needed = ProxorWifi::MonitoringNeeded(ProxorGui::dataStore->ssid_on_demand_enabled,
                                                     ProxorGui::dataStore->ssid_trigger_list, hosts);
    if (needed != wifi_monitor->isActive()) {
        wifi_monitor->setActive(needed);
        if (MW_show_log) {
            const QString watchLine = needed ? tr("[Wi-Fi] Watching the Wi-Fi network (On-Demand or \"Skip on SSIDs\" is configured).")
                                             : tr("[Wi-Fi] Stopped watching the Wi-Fi network: nothing uses it.");
#ifdef Q_OS_WIN
            MW_show_log(watchLine);
#else
            ProxorGui_log::WriteDiagnostic(watchLine);
#endif
        }
    }

    const auto perm = ProxorWifi::CurrentWifiPermission();
    bool askAtFirstStart = false;
#ifdef Q_OS_MACOS
    // macOS asks for Location once at the first start even before On-Demand is configured,
    // so the permission is not first requested in the middle of setting On-Demand up.
    askAtFirstStart = !ProxorGui::dataStore->wifi_permission_prompted && perm == ProxorWifi::PermissionState::NotDetermined;
#endif
    switch (ProxorWifi::DecidePermissionPrompt(perm, needed || askAtFirstStart, wifi_permission_asked)) {
        case ProxorWifi::PermissionPrompt::ExplainThenRequest: {
            wifi_permission_asked = true;
#ifdef Q_OS_MACOS
            ProxorGui::dataStore->wifi_permission_prompted = true;
            ProxorGui::dataStore->Save();
#endif
#ifdef Q_OS_MACOS
            // Straight to the native macOS Location prompt; its text comes from Info.plist.
            QTimer::singleShot(0, this, [this] {
                ProxorWifi::RequestWifiPermission(this, [this](ProxorWifi::PermissionState s) {
                    if (MW_show_log) {
                        ProxorGui_log::WriteDiagnostic(tr("[Wi-Fi] Permission answer: %1")
                                        .arg(s == ProxorWifi::PermissionState::Granted ? tr("allowed") : ProxorWifi::DescribePermission(s)));
                    }
                    wifi_monitor->refreshNow();
                });
            });
            break;
#endif
            QTimer::singleShot(0, this, [this, perm] {
                QMessageBox box(GetMessageBoxParent());
                box.setIcon(QMessageBox::Information);
                box.setWindowTitle(tr("Wi-Fi access"));
                box.setText(ProxorWifi::DescribePermission(perm));
                auto *cont = box.addButton(tr("Continue"), QMessageBox::AcceptRole);
                box.addButton(tr("Not Now"), QMessageBox::RejectRole);
                box.exec();
                if (box.clickedButton() == cont) {
                    ProxorWifi::RequestWifiPermission(this, [this](ProxorWifi::PermissionState s) {
                        if (MW_show_log) {
                            ProxorGui_log::WriteDiagnostic(tr("[Wi-Fi] Permission answer: %1")
                                            .arg(s == ProxorWifi::PermissionState::Granted ? tr("allowed") : ProxorWifi::DescribePermission(s)));
                        }
                        wifi_monitor->refreshNow();
                    });
                } else if (MW_show_log) {
                    MW_show_log(tr("[Wi-Fi] Permission not requested; On-Demand cannot see the Wi-Fi network until it is allowed (Settings > On-Demand)."));
                }
            });
            break;
        }
        case ProxorWifi::PermissionPrompt::PointToSettings:
            if (!wifi_settings_hint_logged) {
                wifi_settings_hint_logged = true;
                if (MW_show_log) MW_show_log("[Wi-Fi] " + ProxorWifi::DescribePermission(perm));
            }
            break;
        case ProxorWifi::PermissionPrompt::None:
            break;
    }
}

void MainWindow::onWifiReadingChanged(const ProxorWifi::WifiReading &reading) {
    const auto text = ProxorWifi::DescribeReading(reading);
    if (text == wifi_last_logged_status) return;
    wifi_last_logged_status = text;
#ifdef Q_OS_WIN
    if (MW_show_log) MW_show_log("[Wi-Fi] " + text);
#else
    // Window log only for what needs the user's attention; the usual SSID changes go to the log file.
    const bool needsAttention = reading.state == ProxorWifi::ReadState::PermissionNeeded ||
                                reading.state == ProxorWifi::ReadState::Unavailable;
    if (needsAttention && wifi_monitor != nullptr && wifi_monitor->isActive()) {
        if (MW_show_log) MW_show_log("[Wi-Fi] " + text);
    } else {
        ProxorGui_log::WriteDiagnostic("[Wi-Fi] " + text);
    }
#endif
}

void MainWindow::onWifiSsidChanged(const QString &ssid) {
    const QString previous = wifi_hosts_ssid;
    wifi_hosts_ssid = ssid;
    const bool viaTrigger = started_via_ssid_trigger;
    const int startedId = ProxorGui::dataStore->started_id;

    if (applyOnDemandForSsid(ssid)) return;

    if (startup_tun_pending || startup_tun_failed) return;
    if (ProxorGui::dataStore->started_id < 0) return;
    if (ProxorGui::dataStore->routing == nullptr) return;
    if (!ProxorWifi::HostsSkipDiffers(ProxorGui::dataStore->routing->hosts_mapping, previous, ssid)) return;

    MW_show_log(tr("[Hosts] Wi-Fi network changed to %1: restarting the profile so \"Skip on SSIDs\" applies.")
                    .arg(ssid.isEmpty() ? tr("no Wi-Fi network") : "\"" + ssid + "\""));
    proxor_start(startedId, viaTrigger);
}

// The Wi-Fi name is usually read before the startup Tun hold is released, and applyOnDemandForSsid refuses
// while the hold is on. Evaluate the current network again once the hold is gone, so a trigger network
// the app starts on is not missed until the next network change.
void MainWindow::applyOnDemandAfterStartup() {
    QTimer::singleShot(0, this, [this] {
        if (!wifi_hosts_ssid.isEmpty()) applyOnDemandForSsid(wifi_hosts_ssid);
    });
}

bool MainWindow::applyOnDemandForSsid(const QString &ssid) {
    if (startup_tun_pending || startup_tun_failed) return false;
    if (!ProxorGui::dataStore->ssid_on_demand_enabled) return false;

    bool isTrigger = !ssid.isEmpty() &&
                     ProxorGui::dataStore->ssid_trigger_list.contains(ssid, Qt::CaseSensitive);

    if (isTrigger) {
        if (ProxorGui::dataStore->started_id >= 0) return false;
        if (auto_start_consumed_ssid == ssid) return false;

        auto targetProfile = resolveSsidOnDemandProfile();
        if (targetProfile == nullptr) {
            MW_show_log(tr("[On-Demand] Trigger SSID \"%1\" detected but no target profile could be resolved").arg(ssid));
            return false;
        }

        auto_start_consumed_ssid = ssid;
        MW_show_log(tr("[On-Demand] Trigger SSID \"%1\" detected — starting profile %2")
                        .arg(ssid, targetProfile->DisplayTypeAndNameSummary()));
        proxor_start(targetProfile->id, true);
        return true;
    } else {
        auto_start_consumed_ssid.clear();
        if (started_via_ssid_trigger && ProxorGui::dataStore->started_id >= 0) {
            wakeDropRestore();
            MW_show_log(tr("[On-Demand] Non-trigger SSID \"%1\" — stopping proxy").arg(ssid));
            proxor_stop(false, false);
            return true;
        }
    }
    return false;
}

// Group tab manage

QList<int> visibleGroupTabOrder() {
    QList<int> visibleIds;
    for (const auto gid: ProxorGui::profileManager->groupsTabOrder) {
        const auto group = ProxorGui::profileManager->GetGroup(gid);
        if (group != nullptr && !group->archive) {
            visibleIds << gid;
        }
    }
    return visibleIds;
}

inline int tabIndex2GroupId(int index) {
    const auto visibleIds = visibleGroupTabOrder();
    if (index < 0 || visibleIds.length() <= index) return -1;
    return visibleIds[index];
}

inline int groupId2TabIndex(int gid) {
    const auto visibleIds = visibleGroupTabOrder();
    for (int key = 0; key < visibleIds.count(); key++) {
        if (visibleIds[key] == gid) return key;
    }
    return -1;
}

bool canReuseGroupTabs(QTabWidget *tabWidget, const QList<int> &groupsTabOrder) {
    if (tabWidget == nullptr) return false;
    Q_UNUSED(groupsTabOrder);
    const auto visibleIds = visibleGroupTabOrder();
    const int expectedCount = visibleIds.count() + 1;
    if (tabWidget->count() != expectedCount) return false;

    auto *tabBar = tabWidget->tabBar();
    if (tabBar == nullptr) return false;
    if (tabBar->tabData(expectedCount - 1).toInt() != kAddGroupTabId) return false;

    for (int i = 0; i < visibleIds.count(); i++) {
        if (tabBar->tabData(i).toInt() != visibleIds[i]) return false;
        auto *page = tabWidget->widget(i);
        if (page == nullptr || page->layout() == nullptr) return false;
    }
    return true;
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if (ProxorGui::dataStore->refreshing_group_list) return;
    if (index < 0 || index >= ui->tabWidget->count()) return;
    if (ui->tabWidget->tabBar()->tabData(index).toInt() == kAddGroupTabId) {
        auto ent = ProxorGui::ProfileManager::NewGroup();
        auto dialog = new DialogEditGroup(ent, this);
        connect(dialog, &QDialog::finished, this, [=] {
            if (dialog->result() == QDialog::Accepted) {
                ProxorGui::profileManager->AddGroup(ent);
                refresh_groups();
                if (!ent->url.trimmed().isEmpty()) {
                    if (startup_tun_pending || startup_tun_failed) return;
                    ProxorGui_sub::groupUpdater->AsyncUpdate(ent->url, ent->id);
                }
            } else {
                auto currentIndex = groupId2TabIndex(ProxorGui::dataStore->current_group);
                if (currentIndex >= 0 && currentIndex < ui->tabWidget->count()) {
                    ui->tabWidget->setCurrentIndex(currentIndex);
                }
            }
            dialog->deleteLater();
        });
        dialog->show();
        auto currentIndex = groupId2TabIndex(ProxorGui::dataStore->current_group);
        if (currentIndex >= 0 && currentIndex < ui->tabWidget->count()) {
            ui->tabWidget->setCurrentIndex(currentIndex);
        }
        return;
    }
    const int gid = tabIndex2GroupId(index);
    if (gid < 0 || gid == ProxorGui::dataStore->current_group) return;
    show_group(gid);
}

void MainWindow::on_down_tab_currentChanged(int index) {
    Q_UNUSED(index);
    update_connection_statistics_polling_state();
}

bool MainWindow::should_refresh_connection_statistics() const {
    return conn_stats_tab_active.load(std::memory_order_relaxed) &&
           conn_stats_window_visible.load(std::memory_order_relaxed);
}

void MainWindow::update_connection_statistics_polling_state() {
    const bool tabActive = ui != nullptr && ui->down_tab->currentWidget() == ui->tab_2;
    const bool windowVisible = isVisible() &&
                               !isMinimized() &&
                               QApplication::applicationState() == Qt::ApplicationActive;
    conn_stats_tab_active.store(tabActive, std::memory_order_relaxed);
    conn_stats_window_visible.store(windowVisible, std::memory_order_relaxed);
}

void MainWindow::queue_resume_subscription_check() {
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    if (wakeOwnsSubscriptions()) return;
#endif
    if (subscription_resume_check_pending || !UI_has_scheduled_subscription_updates()) return;
    subscription_resume_check_pending = true;
    setTimeout([this] {
        subscription_resume_check_pending = false;
        if (!startup_tun_pending && !startup_tun_failed && !UI_subscription_updates_running()) {
            UI_update_due_groups_on_timer();
        }
    }, this, 2000);
}

void MainWindow::update_quota_display() {
    if (m_quotaLabel == nullptr) return;
    const auto text = ProxyListModel::quotaText(ProxorGui::profileManager->CurrentGroup());
    m_quotaLabel->setText(text);
    m_quotaLabel->setVisible(!text.isEmpty());
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
    if (group->archive) {
        ProxorGui::dataStore->refreshing_group = false;
        return;
    }

    if (ProxorGui::dataStore->current_group != gid) {
        ProxorGui::dataStore->current_group = gid;
        ProxorGui::dataStore->Save();
    }
    const int tabIndex = groupId2TabIndex(gid);
    if (tabIndex < 0 || tabIndex >= ui->tabWidget->count()) {
        ProxorGui::dataStore->refreshing_group = false;
        return;
    }
    ui->tabWidget->widget(tabIndex)->layout()->addWidget(ui->proxyListTable);

    // 列宽是否可调
    auto *hdr = ui->proxyListTable->horizontalHeader();
    if (group->manually_column_width) {
        static const int colDefaultWidths[] = {0, 160, 100, 200, 90, 120};
        static const int colMinWidths[]     = {0,  75,  60,  90, 60, 120};
        static const int colMaxWidths[]     = {0, 600, 200, 600, 200, 400};
        hdr->setSectionResizeMode(0, QHeaderView::Fixed);
        hdr->resizeSection(0, toggleColumnWidth);
        const bool firstActivation = group->column_width.isEmpty();
        for (int i = 1; i <= 5; i++) {
            int size = group->column_width.value(i);
            if (size <= 0) size = colDefaultWidths[i];
            size = std::max(size, colMinWidths[i]);
            size = std::min(size, colMaxWidths[i]);
            hdr->setSectionResizeMode(i, QHeaderView::Interactive);
            hdr->resizeSection(i, size);
        }
        if (firstActivation) {
            group->column_width.clear();
            for (int i = 0; i < hdr->count(); i++)
                group->column_width.push_back(hdr->sectionSize(i));
            ProxorGui::profileManager->SaveGroup(group);
        }
    } else {
        hdr->setSectionResizeMode(0, QHeaderView::Fixed);
        hdr->resizeSection(0, toggleColumnWidth);
        hdr->setSectionResizeMode(1, QHeaderView::Stretch);
        hdr->setSectionResizeMode(2, QHeaderView::Fixed);
        hdr->resizeSection(2, 100);
        hdr->setSectionResizeMode(3, QHeaderView::Stretch);
        hdr->setSectionResizeMode(4, QHeaderView::ResizeToContents);
        hdr->setSectionResizeMode(5, QHeaderView::Fixed);
        hdr->resizeSection(5, 120);
    }

    update_quota_display();

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
#ifdef Q_OS_MACOS
        // The colored/monochrome menu-bar icon setting applies without a restart.
        icon_status = -1;
        refresh_status();
#endif
        auto suggestRestartProxy = ProxorGui::dataStore->Save();
        refreshWifiMonitoring();
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
            refresh_groups();
            refresh_proxy_list();
            if (!info.contains("dingyue")) {
                const int addedCount = ProxorGui::dataStore->imported_count;
                // Nothing parsed (e.g. unrelated clipboard text): say so instead of a bare "Added 0".
                show_log_impl(addedCount > 0 ? tr("Added %1 profile(s)").arg(addedCount) : tr("No profiles were found."));
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

void MainWindow::openSettings(const QString &section) {
    if (dialog_is_using) return;
    dialog_is_using = true;
    auto *dialog = new DialogBasicSettings(this);
    connect(dialog, &QDialog::finished, this, [=] {
        dialog->deleteLater();
        dialog_is_using = false;
    });
    if (!section.isEmpty()) dialog->selectSection(section);
    dialog->show();
}

void MainWindow::on_menu_basic_settings_triggered() {
    openSettings();
}

void MainWindow::on_menu_manage_groups_triggered() {
    USE_DIALOG(DialogManageGroups)
}

void MainWindow::on_menu_routing_settings_triggered() {
    openSettings(tr("Routing"));
}

void MainWindow::on_menu_vpn_settings_triggered() {
    openSettings(tr("Tun"));
}

void MainWindow::on_menu_ssid_settings_triggered() {
    openSettings(tr("On-Demand"));
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    openSettings(tr("Hotkeys"));
}

void MainWindow::on_menu_about_triggered() {
    auto *dialog = new QDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(tr("About %1").arg(software_name));
    dialog->setWindowIcon(windowIcon());
    dialog->setModal(true);
    dialog->resize(420, 260);

    auto *mainLayout = new QVBoxLayout(dialog);
    mainLayout->setContentsMargins(16, 16, 16, 12);
    mainLayout->setSpacing(12);

    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(12);
    auto *iconLabel = new QLabel(dialog);
    iconLabel->setPixmap(windowIcon().pixmap(48, 48));
    iconLabel->setFixedSize(48, 48);
    auto *titleLayout = new QVBoxLayout();
    titleLayout->setSpacing(2);
    auto *titleLabel = new QLabel(software_name, dialog);
    auto titleFont = titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    auto *versionLabel = new QLabel(tr("Version %1").arg(QStringLiteral(NKR_VERSION)), dialog);
    titleLayout->addWidget(titleLabel);
    titleLayout->addWidget(versionLabel);
    titleLayout->addStretch(1);
    headerLayout->addWidget(iconLabel);
    headerLayout->addLayout(titleLayout, 1);
    mainLayout->addLayout(headerLayout);

    auto *infoGrid = new QGridLayout();
    infoGrid->setHorizontalSpacing(16);
    infoGrid->setVerticalSpacing(6);
    const auto addInfoRow = [=](int row, const QString &label, const QString &value, const QUrl &url = QUrl()) {
        auto *labelWidget = new QLabel(label, dialog);
        labelWidget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto labelFont = labelWidget->font();
        labelFont.setBold(true);
        labelWidget->setFont(labelFont);
        auto *valueWidget = new QLabel(value, dialog);
        valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
        if (url.isValid()) {
            valueWidget->setText(QStringLiteral("<a href=\"%1\">%2</a>").arg(url.toString(), value.toHtmlEscaped()));
            valueWidget->setTextFormat(Qt::RichText);
            valueWidget->setTextInteractionFlags(Qt::TextBrowserInteraction);
            valueWidget->setOpenExternalLinks(true);
        }
        infoGrid->addWidget(labelWidget, row, 0);
        infoGrid->addWidget(valueWidget, row, 1);
    };
    addInfoRow(0, tr("Repository"), projectRepoSlug(), projectUrl());
    addInfoRow(1, tr("Maintainer"), QString::fromLatin1(kProjectOwner), maintainerUrl());
    addInfoRow(2, tr("License"), tr("GPL v3"));
    addInfoRow(3, tr("Qt"), QString::fromLatin1(qVersion()));
    addInfoRow(4, tr("OS"), QSysInfo::prettyProductName());
    mainLayout->addLayout(infoGrid);
    mainLayout->addStretch(1);

    auto *buttons = new QDialogButtonBox(dialog);
    auto *checkUpdates = buttons->addButton(tr("Check for Updates"), QDialogButtonBox::ActionRole);
    auto *reportBug = buttons->addButton(tr("Report Bug"), QDialogButtonBox::ActionRole);
    auto *donate = buttons->addButton(tr("Donate"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Ok);
    connect(checkUpdates, &QPushButton::clicked, this, [=] {
        runOnNewThread([=] { CheckUpdate(); });
    });
    connect(reportBug, &QPushButton::clicked, dialog, [] {
        QDesktopServices::openUrl(bugReportUrl());
    });
    connect(donate, &QPushButton::clicked, dialog, [] {
        QDesktopServices::openUrl(sponsorUrl());
    });
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    mainLayout->addWidget(buttons);

    dialog->show();
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
#ifdef Q_OS_MACOS
    // Restore the session as it was when Proxor quit: stopped by hand before quitting means no
    // automatic start next time (the remembered id would otherwise revive an older profile).
    if (ProxorGui::dataStore->remember_enable && last_id < 0 && ProxorGui::dataStore->remember_id >= 0) {
        ProxorGui::dataStore->remember_id = -1919;
    }
#endif
    //
    ProxorGui::dataStore->Save();
    ProxorGui::profileManager->SaveManager();
    qDebug() << "End of data save";
}

#ifdef Q_OS_MACOS
ProxorPlatform::MacAppUpdateRoute MainWindow::macAppUpdateRoute(PackageMode mode) const {
    const auto bundle = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
    const auto script = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath())
                                            .filePath(QString::fromLatin1(ProxorPlatform::kMacAppUpdateScriptFromMacOSDir)));
    ProxorPlatform::MacAppUpdateProbe probe;
    probe.macApp = mode == PackageMode::MacApp;
    probe.bundlePath = bundle;
    probe.parentWritable = QFileInfo(QFileInfo(bundle).absolutePath()).isWritable();
    probe.bundleOwnedByUser = QFileInfo(bundle).ownerId() == ::getuid();
    probe.scriptPresent = QFileInfo(script).isFile();
    const auto route = ProxorPlatform::DecideMacAppUpdate(probe, ProxorPlatform::MacAppUpdateBuildCapabilities());
    MW_show_log(QStringLiteral("macOS app update route: %1")
                    .arg(route == ProxorPlatform::MacAppUpdateRoute::InPlace ? QStringLiteral("in place")
                                                                              : QStringLiteral("manual")));
    return route;
}

void MainWindow::macAppUpdateFailed(bool duringDownload, const QString &error, const QString &releaseUrl) {
    MW_show_log(tr("Update failed: %1").arg(error));
    ShowUpdateFailedDialog(this, duringDownload ? UpdateFailureStage::Download : UpdateFailureStage::Install, error,
                           QStringLiteral(NKR_VERSION), releaseUrl);
}

void MainWindow::macAppUpdateStaged() {
    const auto url = mac_app_update_release_url;
    const auto zip = QDir(mac_app_update_zip_dir).filePath(staged_asset_name);
    if (mac_app_update_route != ProxorPlatform::MacAppUpdateRoute::InPlace || !QFileInfo(zip).isFile()) {
        macAppUpdateFailed(false, tr("the downloaded update is missing."), url);
        return;
    }
    const auto bundle = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../.."));
    const auto script = QDir::cleanPath(QDir(QCoreApplication::applicationDirPath())
                                            .filePath(QString::fromLatin1(ProxorPlatform::kMacAppUpdateScriptFromMacOSDir)));
    const auto pid = QCoreApplication::applicationPid();
    // The relauncher runs from a copy outside the bundle that it is about to replace.
    const auto tempScript = QDir::temp().filePath(QStringLiteral("proxor-app-update-%1.sh").arg(pid));
    QFile::remove(tempScript);
    if (!QFile::copy(script, tempScript)) {
        macAppUpdateFailed(false, tr("could not prepare the update installer."), url);
        return;
    }
    const auto result = QDir::current().absoluteFilePath(QString::fromLatin1(ProxorPlatform::kMacAppUpdateResultFileName));
    QFile::remove(result);
    mac_app_update_args = ProxorPlatform::MacAppUpdateInstallArgs(
        tempScript, pid, zip, bundle, result,
        ProxorPlatform::MacAppUpdateRelaunchArgs(QCoreApplication::arguments()));
    update_staged = true;
    exit_reason = 4;
    MW_show_log(tr("Update downloaded; Proxor restarts to install it."));
    on_menu_exit_triggered();
}

void MainWindow::macAppUpdateShowResult() {
    // Leftover copy of the relauncher from an earlier run.
    const auto stale = QDir::temp().entryList({QStringLiteral("proxor-app-update-*.sh")}, QDir::Files);
    for (const auto &name : stale) QFile::remove(QDir::temp().filePath(name));

    const auto file = QDir::current().absoluteFilePath(QString::fromLatin1(ProxorPlatform::kMacAppUpdateResultFileName));
    QFile f(file);
    if (!f.exists()) return;
    QString text;
    if (f.open(QIODevice::ReadOnly)) {
        text = QString::fromUtf8(f.readAll()).trimmed();
        f.close();
    }
    QFile::remove(file);
    const auto parsed = ProxorPlatform::ParseMacAppUpdateResult(text);
    if (!parsed.present) return;
    if (parsed.ok) {
        MW_show_log(tr("Updated to %1.").arg(parsed.version));
        return;
    }
    macAppUpdateFailed(false, parsed.message, QString());
}
#endif
void MainWindow::onUpdateStaged() {
#ifdef Q_OS_LINUX
    // The AppImage owns its own file and replaces it directly -- it must never reach
    // the DecideUpdaterLaunch gate below, since that path only ever leads to the
    // archive-only ./updater, which is not shipped and could not apply a raw
    // .AppImage even if it were.
    if (ProxorGui::CurrentPackageMode() == PackageMode::AppImage) {
        const auto appImagePath = qEnvironmentVariable("APPIMAGE");
        const auto stagedPath = QDir(QFileInfo(appImagePath).absolutePath()).filePath(staged_asset_name);

        AppImageApplyProbe probe{};
        probe.appImagePathKnown = !appImagePath.isEmpty();
        probe.stagedFileExists = QFileInfo::exists(stagedPath);
        probe.targetDirWritable = QFileInfo(QFileInfo(appImagePath).absolutePath()).isWritable();
        probe.targetFileWritable = QFileInfo(appImagePath).isWritable();

        const auto decision = DecideAppImageApply(probe);
        if (!decision.replaceTarget) {
            MessageBoxInfo(software_name, tr("%1 The download was kept at: %2").arg(decision.reason, stagedPath));
            MW_show_log(tr("AppImage update not applied: %1 Download kept at %2").arg(decision.reason, stagedPath));
            return;
        }

        QFile::setPermissions(stagedPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                               QFile::ReadGroup | QFile::ExeGroup |
                                               QFile::ReadOther | QFile::ExeOther);
        // std::rename, not QFile::rename: QFile::rename refuses an existing destination,
        // which would force a remove-then-rename window with no working AppImage at all.
        // rename(2) within one directory replaces atomically.
        if (std::rename(stagedPath.toLocal8Bit().constData(), appImagePath.toLocal8Bit().constData()) != 0) {
            const auto err = errno;
            MW_show_log(tr("Failed to replace the running AppImage (errno %1). Download kept at %2")
                            .arg(err)
                            .arg(stagedPath));
            MessageBoxWarning(software_name, tr("Could not replace the AppImage. The download is still at: %1").arg(stagedPath));
            return;
        }

        update_staged = true;
        this->exit_reason = 2;
        on_menu_exit_triggered();
        return;
    }
#endif
    // The updater is deliberately absent from the native Linux packages, and
    // the package tests assert its absence, so its presence is a runtime
    // fact and not an invariant: check before promising a restart into it.
#ifdef Q_OS_MACOS
    if (mac_app_update_route != ProxorPlatform::MacAppUpdateRoute::Guidance) {
        macAppUpdateStaged();
        return;
    }
#endif
    const auto launch = DecideUpdaterLaunch(ProxorGui::ProbeUpdaterLaunch());
    if (!launch.canLaunch) {
#ifdef Q_OS_WIN
        ShowUpdateFailedDialog(this, UpdateFailureStage::Install, launch.reason, QStringLiteral(NKR_VERSION), update_release_url);
#else
        MessageBoxWarning(software_name, tr("%1 The app will stay open.").arg(launch.reason));
#endif
        MW_show_log(tr("Update downloaded, but it cannot be installed: %1").arg(launch.reason));
        return;
    }
    update_staged = true;
    tray->showMessage(
        tr("Proxor"),
        tr("Update downloaded. Proxor is closing to install it."),
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
            // Re-check: the updater binary or install directory could have
            // changed since onUpdateStaged ran, and promising a restart the
            // app cannot honour is exactly the bug this gate exists to avoid.
            if (DecideUpdaterLaunch(ProxorGui::ProbeUpdaterLaunch()).canLaunch) {
                exit_reason = 1;
            }
        }
        ProxorGui::dataStore->prepare_exit = true;
#ifdef Q_OS_MACOS
        mac_modes->reset();
#endif
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
#ifdef Q_OS_MACOS
    if (exit_reason == 4) {
        if (mac_app_update_args.isEmpty() || !QProcess::startDetached(QStringLiteral("/bin/bash"), mac_app_update_args)) {
            // The installer could not be started: restart this untouched app and let it
            // show the failure from the result file.
            QSaveFile out(QDir::current().absoluteFilePath(QString::fromLatin1(ProxorPlatform::kMacAppUpdateResultFileName)));
            if (out.open(QIODevice::WriteOnly)) {
                out.write("failed start: could not start the update installer.\n");
                out.commit();
            }
            exit_reason = 2;
        }
    }
#endif
    if (exit_reason == 1) {
        // Final check before spawning: the updater is a runtime fact, not an
        // invariant, so don't quit on a promise of a restart that won't happen.
        if (DecideUpdaterLaunch(ProxorGui::ProbeUpdaterLaunch()).canLaunch) {
            QDir::setCurrent(ProxorGui::PackageRootPath());
            QProcess::startDetached(ProxorGui::PackageExecutablePath("updater"), QStringList{});
        }
    } else if (exit_reason == 2 || exit_reason == 3) {
        QDir::setCurrent(ProxorGui::PackageRootPath());

        auto arguments = QCoreApplication::arguments();
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
#ifdef Q_OS_LINUX
        // QApplication::applicationFilePath() under an AppImage lives inside the
        // per-run SquashFS mount, which holds the image just superseded and is torn
        // down when this process exits -- startDetached on it would either relaunch
        // the old version or fail outright. $APPIMAGE is the real file on disk, and
        // after onUpdateStaged()'s rename it is the new version, so prefer it here;
        // this also repairs the "Restart Program" menu action and the Tun restart
        // (exit_reason == 3) under an AppImage, both of which relaunched from the
        // dying mount before this change.
        const auto appImagePath = qEnvironmentVariable("APPIMAGE");
        if (!isLauncher && !appImagePath.isEmpty()) {
            program = appImagePath;
            QDir::setCurrent(QFileInfo(appImagePath).absolutePath());
        }
#endif
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
#ifdef Q_OS_MACOS
    if (mac_status_item) mac_status_item->setVisible(false);
    // QCoreApplication::quit() ends in -[NSApp terminate:], which sends another spontaneous
    // QEvent::Quit; let it through now that the real exit path has finished.
    ProxorMac::AllowQuit();
    // Deliberate lease close: the helper stops Tun and restores the proxy. The window is already hidden,
    // so waiting here (bounded) does not freeze anything visible; queued restores run before it (FIFO).
    MacHelperSvc()->shutdown(3000);
#endif
    QCoreApplication::quit();
}

#define proxor_set_spmode_FAILED \
    refresh_status();          \
    return;

void MainWindow::proxor_set_spmode_system_proxy(bool enable, bool save) {
    // A Flatpak sandbox cannot reach the host's proxy settings, so asking is a
    // guaranteed failure dialog; refuse before even trying.
    if (enable) {
        const auto lifecycle = DecideFlatpakLifecycle(ProxorGui::CurrentPackageMode(), FlatpakLifecycleEntryPoint::MenuToggle);
        if (!lifecycle.allowSystemProxy) {
            MessageBoxWarning(software_name, tr("System Proxy is not available in a Flatpak sandbox."));
            proxor_set_spmode_FAILED
        }
    }
    if (enable != ProxorGui::dataStore->spmode_system_proxy) {
#ifdef Q_OS_MACOS
        if (enable) {
            if (!MacHelperSvc()->isConnected()) {
                // Never wait for the helper on the UI thread: probe asynchronously and re-enter once when it answers.
                const bool restoring = mac_spmode_restoring;
                MacHelperSvc()->probe(this, 1000, [this, save, restoring](const MacHelperProbe &, MacHelperState st) {
                    if (st == MacHelperState::Ready && MacHelperSvc()->isConnected()) {
                        const bool before = mac_spmode_restoring;
                        mac_spmode_restoring = restoring;
                        proxor_set_spmode_system_proxy(true, save);
                        mac_spmode_restoring = before;
                        return;
                    }
                    auto macAction = DecideMacHelperEnable(st);
                    if (macAction == MacHelperEnableAction::Proceed) macAction = MacHelperEnableAction::AskReinstall; // Ready but the connection dropped again
                    if (restoring) { // startup restore: never prompt
                        MW_show_log(tr("System Proxy is remembered, but the Proxor service is not available; leaving it off. Turn on System Proxy to install the service."));
                        refresh_status();
                        return;
                    }
                    macInstallHelperThen(tr("System Proxy"), macAction, [this] { proxor_set_spmode_system_proxy(true); });
                    refresh_status();
                });
                refresh_status();
                return;
            }
            if (ProxorGui::dataStore->started_id < 0) {
                // No profile runs: never point the Mac at a dead port. Applied by proxor_start.
                mac_modes->setSystemProxyParked(true);
                ProxorGui_log::WriteDiagnostic(tr("System Proxy is on; it takes effect when a profile starts."));
            } else {
                // The switch shows on optimistically; macApplySystemProxy reverts it if the helper reports a failure.
                mac_modes->setSystemProxyParked(false);
                macApplySystemProxy(true, save);
            }
        } else {
            mac_modes->setSystemProxyParked(false);
            if (MacHelperSvc()->isConnected() || MacHelperSvc()->lastState() == MacHelperState::Ready) {
                MacHelperSvc()->sysproxyRestore(this, 20000, [this](const MacHelperService::Reply &r) {
                    if (!r.ok) MW_show_log(tr("[Warning] System Proxy restore: %1").arg(r.error));
                });
            }
        }
    }
#else
        if (enable) {
            auto socks_port = ProxorGui::dataStore->inbound_socks_port;
            // The inbound is a single "mixed" listener that answers both HTTP and SOCKS, so the HTTP proxy
            // port is intentionally the same port as SOCKS (not a copy and paste slip).
            auto http_port = ProxorGui::dataStore->inbound_socks_port;
            if (!SetSystemProxy(http_port, socks_port)) {
#ifdef Q_OS_LINUX
                MessageBoxWarning(
                    software_name,
                    tr("System Proxy could not be configured for this desktop environment. Use GNOME, KDE Plasma, or a native installation with Tun mode.")
                );
#else
                MessageBoxWarning(software_name, tr("System Proxy could not be configured."));
#endif
                refresh_status();
                return;
            }
        } else {
            ClearSystemProxy();
        }
    }
#endif

    if (const auto problem = ProxorPlatform::TakeSystemProxyProblem(); !problem.isEmpty()) {
        MW_show_log("[System Proxy] " + problem);
        if (!enable) MessageBoxWarning(software_name, problem);
    }

    if (save) {
        ProxorGui::dataStore->remember_spmode.removeAll("system_proxy");
#ifdef Q_OS_MACOS
        // Like Tun on every platform, System Proxy on macOS is remembered whether or not
        // "Remember last profile" is on (it only comes up once a profile runs).
        if (enable) {
#else
        if (enable && ProxorGui::dataStore->remember_enable) {
#endif
            ProxorGui::dataStore->remember_spmode.append("system_proxy");
        }
        ProxorGui::dataStore->Save();
    }

    ProxorGui::dataStore->spmode_system_proxy = enable;
    refresh_status();
}

#ifdef Q_OS_MACOS
void MainWindow::macInstallHelperThen(const QString &feature, MacHelperEnableAction action, std::function<void()> onReady) {
    if (MacHelperInstaller::InstallInProgress()) {
        // One installer at a time: the switch stays off, exactly like after a declined install.
        ProxorGui_log::WriteDiagnostic(tr("The Proxor service installation is already waiting for your answer."));
        return;
    }
    MW_show_log(tr("Waiting for the administrator password prompt to install the Proxor service..."));
    MacHelperInstaller::ConfirmAndInstall(this, feature, action, [this, feature, onReady = std::move(onReady)](MacAdminScriptResult result) {
        switch (result.outcome) {
        case MacAdminScriptOutcome::Ok:
            MW_show_log(tr("Proxor service installed."));
            onReady();
            break;
        case MacAdminScriptOutcome::Cancelled:
            MW_show_log(tr("%1 was not turned on: the Proxor service was not installed.").arg(feature));
            break;
        case MacAdminScriptOutcome::Failed:
            MessageBoxWarning(software_name, tr("The Proxor service could not be installed: %1").arg(result.reason));
            break;
        }
    });
}
#endif

void MainWindow::proxor_set_spmode_vpn(bool enable, bool save) {
    // A Flatpak sandbox has no TUN device, so asking is a guaranteed failure
    // dialog; refuse before even trying. Distinguish the startup restore path
    // from a manual toggle only for the wording the policy layer may use.
    if (enable) {
        const auto entryPoint = startup_tun_pending
            ? FlatpakLifecycleEntryPoint::StartupRestore
            : FlatpakLifecycleEntryPoint::MenuToggle;
        const auto lifecycle = DecideFlatpakLifecycle(ProxorGui::CurrentPackageMode(), entryPoint);
        if (!lifecycle.allowTun) {
            MessageBoxWarning(software_name, tr("Tun mode is not available in a Flatpak sandbox."));
            proxor_set_spmode_FAILED
        }
    }
    if (enable && startup_tun_failed) {
        startup_tun_failed = false;
        startup_tun_pending = true;
        startup_tun_authorized = false;
    }
    // Turning Tun off means there is no tunnel left to wait for, so startup work that was
    // held back by a failed authorization -- the subscription update after an update
    // restart, for one -- can finally run.
#ifdef Q_OS_MACOS
    if (!enable && startup_tun_failed && !startup_network_work) {
        startup_tun_failed = false;
        mac_tun_failure_reason.clear();
        mac_stop_keeps_remembered_profile = false;
        if (!ProxorGui::dataStore->prepare_exit) {
            MW_show_log(tr("Tun Mode turned off; starting the profile without Tun."));
            resumeDeferredStartupProfile();
        }
    }
#endif
    if (!enable && startup_tun_failed && startup_network_work) {
        startup_tun_failed = false;
        MW_show_log(tr("Tun mode turned off; running the startup work that was waiting for it."));
        auto startupWork = std::move(startup_network_work);
        startupWork();
#ifdef Q_OS_MACOS
        mac_tun_failure_reason.clear();
        mac_stop_keeps_remembered_profile = false;
        if (!ProxorGui::dataStore->prepare_exit) resumeDeferredStartupProfile();
#endif
    }
    if (enable != ProxorGui::dataStore->spmode_vpn) {
        if (enable) {
            if (ProxorGui::UseInternalTun()) {
                bool requestPermission = !ProxorGui::IsAdmin();
                if (requestPermission) {
#ifdef Q_OS_LINUX
                    if (!Linux_HavePkexec()) {
                        MessageBoxWarning(software_name, tr("Tun Mode needs PolicyKit (pkexec). Install it and try again."));
                        proxor_set_spmode_FAILED
                    }
                    if (!Linux_HaveSetcap()) {
                        MessageBoxWarning(software_name, tr("Tun Mode needs libcap tools (setcap). Install them and try again."));
                        proxor_set_spmode_FAILED
                    }
                    auto ret = Linux_Pkexec_SetCapString(ProxorGui::FindProxorCoreRealPath(), "cap_net_admin=ep");
                    if (ret == 0) {
                        MW_show_log(tr("Restarting Proxor to apply Tun permissions."));
                        this->exit_reason = 3;
                        on_menu_exit_triggered();
                    } else {
                        // Technical detail goes to the log; the dialog only says what the user can do.
                        MW_show_log(tr("Tun Mode: granting cap_net_admin to proxor_core failed (setcap exit code %1). The authorization dialog may have been cancelled, or the installation may not allow file capabilities.").arg(ret));
                        MessageBoxWarning(software_name, tr("Tun Mode could not get the permission it needs. If you cancelled the password prompt, try again."));
                    }
#endif
#ifdef Q_OS_WIN
                    if (startup_tun_pending) {
                        // Restoring a remembered Tun at launch: a modal at every login is noise, so log only.
                        MW_show_log(tr("Tun Mode was not restored: run Proxor as administrator."));
                    } else {
                        auto n = QMessageBox::warning(GetMessageBoxParent(), tr("Tun Mode"), tr("Tun needs administrator rights. Restart Proxor as administrator?"), QMessageBox::Yes | QMessageBox::No);
                        if (n == QMessageBox::Yes) {
                            this->exit_reason = 3;
                            on_menu_exit_triggered();
                        }
                    }
#endif
                    proxor_set_spmode_FAILED
                }
            } else {
#ifdef Q_OS_MACOS
                if (!MacHelperSvc()->isConnected()) {
                    // Never wait for the helper on the UI thread: probe asynchronously and re-enter once when it answers.
                    // The check box stays off meanwhile (refresh_status via the FAILED return).
                    const bool restoring = mac_spmode_restoring;
                    MacHelperSvc()->probe(this, 1000, [this, save, restoring](const MacHelperProbe &, MacHelperState st) {
                        if (st == MacHelperState::Ready && MacHelperSvc()->isConnected()) {
                            const bool before = mac_spmode_restoring;
                            mac_spmode_restoring = restoring;
                            proxor_set_spmode_vpn(true, save);
                            mac_spmode_restoring = before;
                            return;
                        }
                        auto macAction = DecideMacHelperEnable(st);
                        if (macAction == MacHelperEnableAction::Proceed) macAction = MacHelperEnableAction::AskReinstall; // Ready but the connection dropped again
                        if (startup_tun_pending) { // should be impossible (startup gated), but never deadlock
                            startup_tun_pending = false;
                            startup_tun_authorized = false;
                            applyOnDemandAfterStartup();
                            MW_show_log(DecideMacTunStartup(true, st).logLine);
                            if (!ProxorGui::dataStore->prepare_exit) resumeDeferredStartupProfile();
                            if (startup_network_work) {
                                auto w = std::move(startup_network_work);
                                w();
                            }
                            refresh_status();
                            return;
                        }
                        macInstallHelperThen(tr("Tun Mode"), macAction, [this] { proxor_set_spmode_vpn(true); });
                        refresh_status(); // the check box stays off until the service is installed
                    });
                    proxor_set_spmode_FAILED
                }
#endif
#ifdef Q_OS_LINUX
                if (!Linux_HavePkexec()) {
                    MessageBoxWarning(software_name, tr("Tun Mode needs PolicyKit (pkexec). Install it and try again."));
                    proxor_set_spmode_FAILED
                }
                if (qEnvironmentVariableIsSet("APPIMAGE")) {
                    ProxorGui_log::WriteDiagnostic(tr("AppImage Tun uses a separate privileged compatibility core."));
                }
#endif
                if (ProxorGui::dataStore->need_keep_vpn_off) {
                    MessageBoxWarning(software_name, tr("Current server is incompatible with Tun. Please stop the server first, enable Tun Mode, and then restart."));
                    proxor_set_spmode_FAILED
                }
                const bool restoringProfile = startup_tun_pending && startup_deferred_profile_id >= 0;
                if (ProxorGui::dataStore->started_id >= 0 || restoringProfile) {
#ifdef Q_OS_MACOS
                    mac_tun_request_saves = save; // a failed start un-remembers Tun only when the user asked for it
#endif
                    if (!StartVPNProcess()) {
                        proxor_set_spmode_FAILED
                    }
                } else {
                    // A compatibility TUN forwards traffic through the selected profile's
                    // local SOCKS listener; do not install routes before it exists.
                    MW_show_log(tr("Tun is enabled and will start after a proxy profile is running."));
                    if (startup_tun_pending) {
                        startup_tun_pending = false;
                        startup_tun_authorized = true;
                        applyOnDemandAfterStartup();
                        if (startup_network_work) {
                            auto startupWork = std::move(startup_network_work);
                            startupWork();
                        }
                    }
                }
            }
        } else {
            if (ProxorGui::UseInternalTun()) {
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
    g_tun_enabled_ms = enable ? QDateTime::currentMSecsSinceEpoch() : 0;
    refresh_status();

    if (enable && startup_tun_pending && ProxorGui::UseInternalTun()) {
        completeStartupTunAuthorization();
    }
    if (ProxorGui::UseInternalTun() && ProxorGui::dataStore->started_id >= 0) proxor_start(ProxorGui::dataStore->started_id);
}

void MainWindow::syncWindowsHostsMapping(bool enable) {
#ifdef Q_OS_WIN
    const auto path = windowsHostsPath();
    QFile file(path);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        if (MW_show_log) MW_show_log(tr("[Warning] Failed to read Windows hosts file for hosts mapping."));
        return;
    }
    QTextStream in(&file);
    const auto current = in.readAll();
    file.close();

    auto next = removeProxorHostsBlock(current);
    const auto block = enable ? buildProxorHostsBlock() : QString{};
    if (!block.isEmpty()) {
        if (!next.isEmpty()) next += "\r\n\r\n";
        next += block;
        next += "\r\n";
    } else if (!next.isEmpty()) {
        next += "\r\n";
    }

    if (next == current) return;
    if (!writeTextFile(path, next)) {
        if (MW_show_log) MW_show_log(tr("[Warning] Failed to write Windows hosts file for hosts mapping."));
        return;
    }
    QProcess::execute("ipconfig", {"/flushdns"});
#else
    Q_UNUSED(enable)
#endif
}

void MainWindow::update_tray_speed(qint64 uploadBytesPerSecond, qint64 downloadBytesPerSecond, bool clear) {
#ifdef Q_OS_MACOS
    if (mac_status_item == nullptr) return;
    if (clear || !ProxorGui::dataStore->tray_speed_view) {
        mac_status_item->setSpeedText(QString());
        return;
    }
    mac_status_item->setSpeedText(ProxorPlatform::FormatTraySpeed(uploadBytesPerSecond, downloadBytesPerSecond));
#else
    Q_UNUSED(uploadBytesPerSecond)
    Q_UNUSED(downloadBytesPerSecond)
    Q_UNUSED(clear)
#endif
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
    if (ProxorGui::dataStore->started_id >= 0) last_started_profile_id = ProxorGui::dataStore->started_id;
    const bool showStopState = running != nullptr || start_pending;
    if (tray_toggle_action != nullptr) tray_toggle_action->setText(showStopState ? tr("Stop") : tr("Start"));
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
    if (icon_status_new != icon_status) {
        auto newIcon = Icon::GetTrayIcon(icon_status_new);
        QApplication::setWindowIcon(newIcon);
        for (QWidget *w : QApplication::topLevelWidgets())
            w->setWindowIcon(newIcon);
    }

    // refresh tray
    if (tray != nullptr) {
        tray->setToolTip(make_title(true));
        if (icon_status_new != icon_status) tray->setIcon(Icon::GetTrayIcon(icon_status_new));
#ifdef Q_OS_MACOS
        if (mac_status_item) {
            mac_status_item->setToolTip(make_title(true));
            mac_status_item->setColored(ProxorGui::dataStore->tray_icon_colored);
            mac_status_item->setActive(icon_status_new != Icon::NONE);
            if (!ProxorGui::dataStore->tray_speed_view) mac_status_item->setSpeedText(QString());
            if (icon_status_new != icon_status) mac_status_item->setIcon(Icon::GetTrayIcon(icon_status_new));
        }
#endif
    }

    icon_status = icon_status_new;
}

// table显示

// refresh_groups -> show_group -> refresh_proxy_list
void MainWindow::refresh_groups() {
    ProxorGui::dataStore->refreshing_group_list = true;
    auto &groupsTabOrder = ProxorGui::profileManager->groupsTabOrder;
    const auto visibleGroups = visibleGroupTabOrder();
    auto *tabBar = ui->tabWidget->tabBar();
    const bool reuseTabs = canReuseGroupTabs(ui->tabWidget, groupsTabOrder);

    if (reuseTabs) {
        for (int index = 0; index < visibleGroups.count(); index++) {
            auto group = ProxorGui::profileManager->GetGroup(visibleGroups[index]);
            if (group == nullptr) continue;
            ui->tabWidget->setTabText(index, groupTabText(group->name));
            tabBar->setTabData(index, group->id);
        }
        tabBar->setTabData(visibleGroups.count(), kAddGroupTabId);
        tabBar->setTabToolTip(visibleGroups.count(), tr("Add group"));
    } else {
        ui->proxyListTable->setParent(nullptr);
        for (int i = ui->tabWidget->count() - 1; i >= 0; i--) {
            auto *page = ui->tabWidget->widget(i);
            ui->tabWidget->removeTab(i);
            if (page != nullptr) page->deleteLater();
        }

        int index = 0;
        for (const auto &gid: visibleGroups) {
            auto group = ProxorGui::profileManager->GetGroup(gid);
            auto widget2 = new QWidget();
            auto layout2 = new QVBoxLayout();
            layout2->setContentsMargins(QMargins());
            layout2->setSpacing(0);
            widget2->setLayout(layout2);
            ui->tabWidget->addTab(widget2, group == nullptr ? QString{} : groupTabText(group->name));
            tabBar->setTabData(index, gid);
            index++;
        }

        auto addGroupWidget = new QWidget();
        auto addGroupLayout = new QVBoxLayout();
        addGroupLayout->setContentsMargins(QMargins());
        addGroupLayout->setSpacing(0);
        addGroupWidget->setLayout(addGroupLayout);
        ui->tabWidget->addTab(addGroupWidget, " +");
        tabBar->setTabData(index, kAddGroupTabId);
        tabBar->setTabToolTip(index, tr("Add group"));
    }

    // show after group changed
    auto currentGroup = ProxorGui::profileManager->CurrentGroup();
    if (currentGroup == nullptr || currentGroup->archive) {
        ProxorGui::dataStore->current_group = -1;
        const int firstVisibleGroup = visibleGroups.isEmpty() ? -1 : visibleGroups.first();
        const int firstVisibleIndex = groupId2TabIndex(firstVisibleGroup);
        if (firstVisibleIndex >= 0) {
            ui->tabWidget->setCurrentIndex(firstVisibleIndex);
            show_group(firstVisibleGroup);
        } else if (ui->tabWidget->count() > 0) {
            ui->tabWidget->setCurrentIndex(ui->tabWidget->count() - 1);
        }
    } else {
        const int currentIndex = groupId2TabIndex(ProxorGui::dataStore->current_group);
        if (currentIndex >= 0) {
            ui->tabWidget->setCurrentIndex(currentIndex);
            show_group(ProxorGui::dataStore->current_group);
        }
    }

    ProxorGui::dataStore->refreshing_group_list = false;
}

void MainWindow::refresh_proxy_list(const int &id) {
    refresh_proxy_list_impl(id, {});
}

void MainWindow::refresh_proxy_list_rows(const QList<int> &ids) {
    if (ids.isEmpty()) return;
    refresh_proxy_list_impl_refresh_data(QSet<int>(ids.begin(), ids.end()));
}

void MainWindow::refresh_proxy_list_impl(const int &id, GroupSortAction groupSortAction) {
    // id < 0 重绘
    if (id < 0) {
        auto group = ProxorGui::profileManager->CurrentGroup();
        if (group == nullptr) return;
        QList<int> orderedIds;
        for (const auto &profile: group->ProfilesWithOrder()) {
            if (profile != nullptr) orderedIds += profile->id;
        }

        switch (groupSortAction.method) {
            case GroupSortMethod::Raw: {
                break;
            }
            case GroupSortMethod::ById: {
                std::sort(orderedIds.begin(), orderedIds.end());
                group->order.clear();
                ProxorGui::profileManager->SaveGroup(group);
                break;
            }
            case GroupSortMethod::ByAddress:
            case GroupSortMethod::ByName:
            case GroupSortMethod::ByLatency:
            case GroupSortMethod::ByType: {
                std::sort(orderedIds.begin(), orderedIds.end(),
                          [=](int a, int b) {
                              QString ms_a;
                              QString ms_b;
                              if (groupSortAction.method == GroupSortMethod::ByType) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->DisplayTypeSummary();
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->DisplayTypeSummary();
                              } else if (groupSortAction.method == GroupSortMethod::ByName) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->summary_name;
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->summary_name;
                              } else if (groupSortAction.method == GroupSortMethod::ByAddress) {
                                  ms_a = ProxorGui::profileManager->GetProfile(a)->DisplayAddressSummary();
                                  ms_b = ProxorGui::profileManager->GetProfile(b)->DisplayAddressSummary();
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
                if (groupSortAction.save_sort) {
                    group->order = orderedIds;
                    ProxorGui::profileManager->SaveGroup(group);
                }
                break;
            }
        }
        proxyListModel->setProfileIds(orderedIds);
    }

    // refresh data
    refresh_proxy_list_impl_refresh_data(id);
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const int &id) {
    if (id < 0) {
        refresh_proxy_list_impl_refresh_data(QSet<int>{});
        return;
    }
    refresh_proxy_list_impl_refresh_data(QSet<int>{id});
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const QSet<int> &ids) {
    auto group = ProxorGui::profileManager->CurrentGroup();
    Q_UNUSED(group)
    proxyListModel->refreshRows(ids);
    apply_proxy_list_search(ui->search->text());

    if (group != nullptr && !group->manually_column_width) {
        auto header = ui->proxyListTable->horizontalHeader();
        if (header->count() > 5) {
            ui->proxyListTable->resizeColumnToContents(5);
            const int trafficWidth = std::max(120, header->sectionSize(5));
            header->resizeSection(5, trafficWidth);
        }
    }
}

void MainWindow::apply_proxy_list_search(const QString &text) {
    ui->proxyListTable->setSearchText(text);
}

// table菜单相关

void MainWindow::on_proxyListTable_doubleClicked(const QModelIndex &index) {
    if (!index.isValid() || index.column() == ProxyListModel::ToggleColumn) return;
    const auto id = index.data(ProxyListModel::ProfileIdRole).toInt();
    if (select_mode) {
        emit profile_selected(id);
        select_mode = false;
        refresh_status();
        return;
    }
    auto dialog = new DialogEditProfile("", id, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_input_triggered() {
    auto dialog = new DialogEditProfile("socks", ProxorGui::dataStore->current_group, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_clipboard_triggered() {
    auto clipboard = QApplication::clipboard()->text();
#ifndef NKR_NO_ZXING
    if (clipboard.trimmed().isEmpty()) {
        const auto *mime = QApplication::clipboard()->mimeData();
        if (mime && mime->hasImage()) {
            on_menu_scan_qr_clipboard_triggered();
            return;
        }
    }
#endif
    ProxorGui_sub::groupUpdater->AsyncUpdate(clipboard);
}

void MainWindow::on_menu_clone_triggered() {
    auto ents = get_now_selected_list();
    if (ents.isEmpty()) return;

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
    if (QMessageBox::question(this, tr("Confirmation"), QString(tr("Remove %1 item(s)?")).arg(ents.count())) ==
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
    QMessageBox mb(QMessageBox::Information, software_name, ents.first()->ToJsonBytes(), QMessageBox::NoButton, this);
    auto *btnEdit   = mb.addButton(tr("Edit"),   QMessageBox::ActionRole);
    auto *btnReload = mb.addButton(tr("Reload"), QMessageBox::ActionRole);
    mb.addButton(tr("OK"), QMessageBox::AcceptRole);
    mb.exec();
    if (mb.clickedButton() == btnEdit) {
        auto dialog = new DialogEditProfile("", ents.first()->id, this);
        connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
    } else if (mb.clickedButton() == btnReload) {
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

    QMessageBox msg(QMessageBox::Question, software_name, tr("Export %1 config").arg(ent->bean->DisplayName()), QMessageBox::NoButton, this);
    auto *btnCore = msg.addButton(tr("Copy core config"), QMessageBox::AcceptRole);
    auto *btnTest = msg.addButton(tr("Copy test config"), QMessageBox::AcceptRole);
    auto *btnCancel = msg.addButton(QMessageBox::Cancel);
    msg.setEscapeButton(btnCancel);
    msg.setDefaultButton(btnCore);
    msg.exec();
    if (msg.clickedButton() != btnCore && msg.clickedButton() != btnTest) return;

    auto result = BuildConfig(ent, msg.clickedButton() == btnTest, false);
    QApplication::clipboard()->setText(QJsonObject2QString(result->coreConfig, false));
    show_log_impl(tr("Config copied"));
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
                MessageBoxWarning(software_name, ex.what());
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
            cb->setText(tr("Proxor Links"));
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

void MainWindow::importQrFromImage(const QImage &image, ProxorPlatform::QrSource source) {
    const auto text = ProxorPlatform::DecodeQrFromImage(image);
    const auto msg = ProxorPlatform::QrScanMessage(source, {!image.isNull(), !text.isEmpty()},
                                                   ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::ScreenQrCapture));
    if (!msg.isEmpty()) {
        MessageBoxInfo(software_name, msg);
        return;
    }
    show_log_impl("QR Code Result:\n" + text);
    ProxorGui_sub::groupUpdater->AsyncUpdate(text);
}

void MainWindow::on_menu_scan_qr_camera_triggered() {
#ifdef NKR_CAMERA_SCAN
    DialogScanCamera dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        if (!dialog.failure().isEmpty()) {
            // The OS asks only once; later denials need this pointer to the right settings page (log only, no extra modal).
            QString hint;
#if defined(Q_OS_MACOS)
            if (dialog.failure().contains(QStringLiteral("not allowed")))
                hint = tr(" Allow Proxor in System Settings > Privacy & Security > Camera.");
#elif defined(Q_OS_WIN)
            if (dialog.failure().contains(QStringLiteral("not allowed")))
                hint = tr(" Allow Proxor in Settings > Privacy & security > Camera.");
#endif
            show_log_impl(tr("Scan QR code with camera: %1.").arg(dialog.failure()) + hint);
        }
        return;
    }
    show_log_impl("QR Code Result:\n" + dialog.text());
    ProxorGui_sub::groupUpdater->AsyncUpdate(dialog.text());
#endif
}

void MainWindow::on_menu_scan_qr_image_triggered() {
    const auto path = QFileDialog::getOpenFileName(this, tr("Select an image with a QR code"), QString(),
                                                   tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty()) return;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    importQrFromImage(reader.read(), ProxorPlatform::QrSource::ImageFile);
}

void MainWindow::on_menu_scan_qr_clipboard_triggered() {
    const auto *mime = QApplication::clipboard()->mimeData();
    QImage img;
    if (mime && mime->hasImage()) img = qvariant_cast<QImage>(mime->imageData());
    importQrFromImage(img, ProxorPlatform::QrSource::ClipboardImage);
}

#ifdef Q_OS_MACOS
// Screen Recording is checked before any capture: without it macOS returns only the wallpaper (phase 53, MAC-QR).
// macOS shows its own prompt, so Proxor adds no dialog of its own; the request is repeated on every attempt.
bool MainWindow::macScreenCaptureReady() {
    using namespace ProxorPlatform;
    const auto d = DecideMacScreenScan(ProxorMac::ScreenCapturePreflight(), false);
    if (d.capture) return true;
    show_log_impl(tr("Scan QR code from screen: the Screen Recording permission is missing. Allow Proxor in System Settings > Privacy & Security > Screen & System Audio Recording."));
    ProxorMac::ScreenCaptureRequest();
    return false;
}

#endif
void MainWindow::on_menu_scan_qr_triggered() {
#ifndef NKR_NO_ZXING
    using namespace ProxorPlatform;
#ifdef Q_OS_MACOS
    if (!macScreenCaptureReady()) return;
    const CapabilityStatus cap{}; // permission granted: a miss is a real "not found"
#else
    const auto cap = CurrentCapability(Capability::ScreenQrCapture);
#endif
    auto offerAlternatives = [this](const QString &message) {
        QMessageBox box(QMessageBox::Information, software_name, message, QMessageBox::Close, this);
        auto *fileBtn = box.addButton(tr("Choose Image File…"), QMessageBox::ActionRole);
        auto *clipBtn = box.addButton(tr("Use Clipboard Image"), QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == fileBtn) on_menu_scan_qr_image_triggered();
        else if (box.clickedButton() == clipBtn) on_menu_scan_qr_clipboard_triggered();
    };
    if (!IsUsable(cap)) {
        offerAlternatives(QrScanMessage(QrSource::Screen, {false, false}, cap));
        return;
    }

    if (SelectScreenCaptureBackend(CurrentPlatformEnvironment()) == ScreenCaptureBackend::Portal) {
        const bool wasVisible = isVisible();
        hide();
        QTimer::singleShot(400, this, [this, cap, offerAlternatives, wasVisible] {
            ProxorDesktop::TakeScreenshot(QString(), this, [this, cap, offerAlternatives, wasVisible](const ProxorDesktop::ScreenshotResult &r) {
                if (wasVisible) show();
                if (r.result.outcome == ProxorDesktop::PortalOutcome::Cancelled) {
                    show_log_impl(tr("Scan QR code from screen: %1").arg(r.result.detail));
                    return;
                }
                if (r.result.outcome != ProxorDesktop::PortalOutcome::Granted) {
                    offerAlternatives(tr("The desktop did not provide a screenshot: %1 Add the QR code from an image file or the clipboard instead.").arg(r.result.detail));
                    return;
                }
                QImageReader reader(r.imagePath);
                reader.setAutoTransform(true);
                const auto image = reader.read();
                if (r.temporary) QFile::remove(r.imagePath);
                else show_log_impl(tr("Scan QR code from screen: the desktop saved the screenshot at %1 and Proxor leaves it there (GNOME keeps screenshots in Pictures/Screenshots). Delete it if you do not need it.").arg(r.imagePath));
                const auto text = DecodeQrFromImage(image);
                const auto msg = QrScanMessage(QrSource::Screen, {!image.isNull(), !text.isEmpty()}, cap);
                if (!msg.isEmpty()) {
                    MessageBoxInfo(software_name, msg);
                    return;
                }
                show_log_impl("QR Code Result:\n" + text);
                ProxorGui_sub::groupUpdater->AsyncUpdate(text);
            });
        });
        return;
    }

    // Let the window disappear from the screenshot without blocking the event loop, and only bring it back
    // if it was visible before (a window hidden in the tray stays hidden).
    const bool wasVisible = isVisible();
    hide();
    QTimer::singleShot(400, this, [this, cap, wasVisible] {
        // Primary screen first, then every other screen.
        QList<QScreen *> screens = QGuiApplication::screens();
        if (auto *primary = QGuiApplication::primaryScreen()) {
            screens.removeAll(primary);
            screens.prepend(primary);
        }
        bool anyImage = false;
        QString text;
        for (auto *screen : screens) {
            const auto g = screen->geometry();
            const auto image = screen->grabWindow(0, g.x(), g.y(), g.width(), g.height()).toImage();
            if (image.isNull()) continue;
            anyImage = true;
            text = ProxorPlatform::DecodeQrFromImage(image);
            if (!text.isEmpty()) break;
        }

        if (wasVisible) show();

#ifdef Q_OS_MACOS
        if (!anyImage) { // preflight said granted but the grab is empty: still the permission, not "not found"
            show_log_impl(tr("Scan QR code from screen: the Screen Recording permission is missing. Allow Proxor in System Settings > Privacy & Security > Screen & System Audio Recording."));
            return;
        }
#endif
        const auto msg = ProxorPlatform::QrScanMessage(ProxorPlatform::QrSource::Screen, {anyImage, !text.isEmpty()}, cap);
        if (!msg.isEmpty()) {
            MessageBoxInfo(software_name, msg);
        } else {
            show_log_impl("QR Code Result:\n" + text);
            ProxorGui_sub::groupUpdater->AsyncUpdate(text);
        }
    });
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
            TM_auto_update_subsctiption_Reset_Minute(ProxorGui::dataStore->sub_auto_update);
            refresh_groups();
            if (!ent->url.trimmed().isEmpty()) {
                ProxorGui_sub::groupUpdater->AsyncUpdate(ent->url, ent->id);
            }
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
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 item(s)?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
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
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 item(s)?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            ProxorGui::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_resolve_domain_triggered() {
    auto profiles_all = get_selected_or_group();
    if (profiles_all.isEmpty()) return;

    QList<std::shared_ptr<ProxorGui::ProxyEntity>> profiles;
    int skipped = 0;
    for (const auto &profile: profiles_all) {
        if (profile->bean->CanResolveDomainToIP()) {
            profiles << profile;
        } else {
            skipped++;
        }
    }
    if (profiles.isEmpty()) return;

    if (QMessageBox::question(this,
                              tr("Resolve Domain"),
                              tr("Resolve domain names to IP addresses?")) != QMessageBox::StandardButton::Yes) {
        return;
    }
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    ProxorGui::dataStore->resolve_count = profiles.count();
    auto resolved_count = std::make_shared<int>(0);
    auto failed_count = std::make_shared<int>(0);

    for (const auto &profile: profiles) {
        profile->bean->ResolveDomainToIP([=](bool ok) {
            ProxorGui::profileManager->SaveProfile(profile);
            if (ok) {
                (*resolved_count)++;
            } else {
                (*failed_count)++;
            }
            if (--ProxorGui::dataStore->resolve_count != 0) return;
            refresh_proxy_list();
            mw_sub_updating = false;
            MW_show_log(tr("<<<<<<<< Resolved %1 / Skipped %2 / Failed %3")
                            .arg(*resolved_count)
                            .arg(skipped)
                            .arg(*failed_count));
        });
    }
}

void MainWindow::on_proxyListTable_customContextMenuRequested(const QPoint &pos) {
#ifdef Q_OS_MACOS
    ProxorMac::PopupMenuAt(ui->menu_server, ui->proxyListTable->viewport(), pos); // native NSMenu
#else
    ui->menu_server->popup(ui->proxyListTable->viewport()->mapToGlobal(pos)); // 弹出菜单
#endif
}

QList<std::shared_ptr<ProxorGui::ProxyEntity>> MainWindow::get_now_selected_list() {
    QList<std::shared_ptr<ProxorGui::ProxyEntity>> list;
    for (const auto id: ui->proxyListTable->selectedProfileIds()) {
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
    // The button shows "Stop" while a start is pending, so it must cancel that start instead of
    // asking for another one.
    if (ProxorGui::dataStore->started_id >= 0 || start_pending) {
        wakeDropRestore();
        proxor_stop();
        return;
    }

    auto group = ProxorGui::profileManager->CurrentGroup();
    if (group == nullptr || group->archive) return;

    auto toggleProxyIds = get_toggle_proxy_ids(group);
    if (toggleProxyIds.isEmpty()) {
        // Nothing marked in the Toggle column: reconnect the profile that ran last, when there is one.
        const int fallbackId = last_started_profile_id >= 0 ? last_started_profile_id : ProxorGui::dataStore->remember_id;
        if (fallbackId >= 0 && ProxorGui::profileManager->GetProfile(fallbackId) != nullptr) {
            proxor_start(fallbackId);
            return;
        }
        MessageBoxWarning(software_name, tr("Select a profile first."));
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
    const int overflow = m_logLines.size() - ProxorGui::dataStore->max_log_line;
    if (overflow > 0) {
        m_logLines.erase(m_logLines.begin(), m_logLines.begin() + overflow);
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

#ifdef Q_OS_MACOS
    ProxorMac::PopupMenuAt(menu, ui->masterLogBrowser->viewport(), pos); // native NSMenu
#else
    menu->exec(ui->masterLogBrowser->viewport()->mapToGlobal(pos)); // 弹出菜单
#endif
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

    QVector<QJsonObject> visibleItems;
    visibleItems.reserve(arr.size());
    for (const auto &_item: arr) {
        auto item = _item.toObject();
        if (ProxorGui::dataStore->ignoreConnTag.contains(item["Tag"].toString())) continue;
        visibleItems.append(item);
    }

    ui->tableWidget_conn->setRowCount(visibleItems.size());

    int row = -1;
    for (const auto &item: visibleItems) {

        row++;

        auto f0 = std::make_unique<SortableTableWidgetItem>();
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
        f_status->setData(Qt::UserRole, item["Start"].toVariant().toLongLong());
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
        f->setText(target2.isEmpty() ? target1 : target1 + " " + target2);
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
        f->setData(Qt::UserRole, upload + download);
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
inline std::unique_ptr<ProxorDesktop::GlobalShortcutSession> PortalHotkeySession;

QStringList MainWindow::RegisterHotkey(bool unregister) {
    // The shared_ptr destructor unregisters the OS hotkey; the objects are not QObject-parented.
    RegisteredHotkey.clear();
    // Closes the old portal session (asynchronously, so shutdown never blocks the UI thread).
    PortalHotkeySession.reset();
    if (unregister) return {};

    QList<ProxorPlatform::HotkeyBinding> bindings{
        {tr("Show main window"), ProxorGui::dataStore->hotkey_mainwindow},
        {tr("Manage groups"), ProxorGui::dataStore->hotkey_group},
        {tr("Routing settings"), ProxorGui::dataStore->hotkey_route},
        {tr("System proxy menu"), ProxorGui::dataStore->hotkey_system_proxy_menu},
    };
    auto plan = ProxorPlatform::PlanHotkeyRegistration(
        bindings, ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::GlobalHotkeys));

    if (ProxorPlatform::SelectHotkeyBackend(ProxorPlatform::CurrentPlatformEnvironment()) == ProxorPlatform::HotkeyBackend::Portal) {
        // Stable ids per action; the desktop remembers bindings by id.
        const QHash<QString, QString> idForAction{
            {tr("Show main window"), "show-main-window"}, {tr("Manage groups"), "manage-groups"},
            {tr("Routing settings"), "routing-settings"}, {tr("System proxy menu"), "system-proxy-menu"}};
        QList<ProxorDesktop::ShortcutRequest> requests;
        QHash<QString, QString> sequenceForId, actionForId;
        for (const auto &b : plan.toRegister) {
            const auto id = idForAction.value(b.action);
            requests << ProxorDesktop::ShortcutRequest{id, b.action, ProxorPlatform::PortalTriggerFromKeySequence(b.sequence)};
            sequenceForId.insert(id, b.sequence);
            actionForId.insert(id, b.action);
        }
        if (!requests.isEmpty()) {
            PortalHotkeySession = ProxorDesktop::CreateGlobalShortcutSession(this);
            if (PortalHotkeySession) {
                PortalHotkeySession->onActivated = [this, sequenceForId](const QString &id) { HotkeyEvent(sequenceForId.value(id)); };
                PortalHotkeySession->bind(requests, QString(), [requests, actionForId](const QList<ProxorDesktop::BoundShortcut> &bound, const ProxorDesktop::PortalResult &r) {
                    if (!MW_show_log) return;
                    if (r.outcome != ProxorDesktop::PortalOutcome::Granted) {
                        MW_show_log(QObject::tr("Hotkeys: %1").arg(r.detail));
                        return;
                    }
                    for (const auto &req : requests) {
                        auto it = std::find_if(bound.begin(), bound.end(), [&](const auto &x) { return x.id == req.id; });
                        if (it == bound.end())
                            MW_show_log(QObject::tr("Hotkeys: the desktop did not bind \"%1\". Set it in the desktop's keyboard shortcut settings.").arg(actionForId.value(req.id)));
                        else
                            MW_show_log(QObject::tr("Hotkeys: \"%1\" is bound to %2 by the desktop.")
                                            .arg(actionForId.value(req.id), it->triggerDescription.isEmpty() ? QObject::tr("a key you choose in the desktop's settings") : it->triggerDescription));
                    }
                });
            } else {
                plan.problems << tr("Global hotkeys: the desktop's GlobalShortcuts portal could not be used.");
            }
        }
        return plan.problems;
    }

    for (const auto &b : plan.toRegister) {
        QKeySequence k(b.sequence);
        if (k.isEmpty()) continue;
        auto hk = std::make_shared<QHotkey>(k, true);
        if (hk->isRegistered()) {
            const QString key = b.sequence;
            RegisteredHotkey += hk;
            connect(hk.get(), &QHotkey::activated, this, [=] { HotkeyEvent(key); });
        } else {
            plan.problems << ProxorPlatform::HotkeyRejectedText(b);
        }
    }
    return plan.problems;
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

QStringList MainWindow::RegisterHotkey(bool unregister) {
    if (unregister) return {};
    QList<ProxorPlatform::HotkeyBinding> bindings{
        {tr("Show main window"), ProxorGui::dataStore->hotkey_mainwindow},
        {tr("Manage groups"), ProxorGui::dataStore->hotkey_group},
        {tr("Routing settings"), ProxorGui::dataStore->hotkey_route},
        {tr("System proxy menu"), ProxorGui::dataStore->hotkey_system_proxy_menu},
    };
    return ProxorPlatform::PlanHotkeyRegistration(
               bindings, ProxorPlatform::CurrentCapability(ProxorPlatform::Capability::GlobalHotkeys))
        .problems;
}

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
    auto scriptPath = ProxorGui::WriteVPNLinuxScript();
    //
#ifdef Q_OS_MACOS
    {
        QFile f(configPath);
        f.open(QIODevice::ReadOnly);
        const auto config = f.readAll();
        vpn_pid = 1; // marker: Tun requested from the helper (set before the async call so a second start is a no-op)
        MacHelperSvc()->tunStart(this, config, ProxorGui::dataStore->inbound_socks_port, 10000, [this](const MacHelperService::Reply &reply) {
            if (!reply.ok) {
                vpn_pid = 0;
                if (startup_tun_pending) {
                    macTunFailed(reply.error);
                    return;
                }
                MessageBoxWarning(software_name, MacTunFailureText(reply.error));
                // Manual Tun after a profile is already up (proxor_start), or the switch was shown on optimistically.
                if (ProxorGui::dataStore->spmode_vpn) proxor_set_spmode_vpn(false, mac_tun_request_saves);
                return;
            }
            if (startup_tun_pending) {
                authorizeStartupTun(); // resumes the deferred profile so the SOCKS port the helper waits for comes up
                if (!mac_tun_ready_timer) {
                    mac_tun_ready_timer = new QTimer(this);
                    mac_tun_ready_timer->setSingleShot(true);
                    connect(mac_tun_ready_timer, &QTimer::timeout, this, [this] {
                        if (startup_tun_pending) {
                            MacHelperSvc()->tunStop(nullptr, 5000, {});
                            macTunFailed(tr("the Tun interface did not come up within 45 seconds"));
                        }
                    });
                }
                mac_tun_ready_timer->start(45000);
            }
        });
        ProxorGui_log::WriteDiagnostic(tr("Tun requested from the Proxor service; waiting for the interface."));
        return true;
    }
#endif
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
    auto corePath = ProxorGui::FindProxorCoreRealPath();
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsSet("APPIMAGE")) {
        const auto runtimeDir = QDir::current().filePath("runtime");
        const auto copiedCorePath = QDir(runtimeDir).filePath("proxor_core");
        if (!QDir().mkpath(runtimeDir) || !QFile::remove(copiedCorePath) && QFile::exists(copiedCorePath) ||
            !QFile::copy(corePath, copiedCorePath)) {
            MessageBoxWarning(software_name, tr("Failed to prepare the AppImage Tun compatibility core."));
            return false;
        }
        QFile::setPermissions(copiedCorePath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        corePath = copiedCorePath;
    }
#endif
    auto vpn_process = new QProcess;
    QProcess::connect(vpn_process, &QProcess::stateChanged, this, [=](QProcess::ProcessState state) {
        if (state == QProcess::NotRunning) {
            if (startup_tun_pending) failStartupTunAuthorization();
            vpn_pid = 0;
            vpn_process->deleteLater();
            GetMainWindow()->proxor_set_spmode_vpn(false);
        }
    });
    //
    vpn_process->setProcessChannelMode(QProcess::SeparateChannels);
#ifndef Q_OS_MACOS
    QStringList vpnArgs{"bash", scriptPath, corePath, configPath, "proxor-tun"};
    // The script only starts after a profile is active or is being restored.
    // Wait for its SOCKS listener before TUN routes system traffic.
    if (ProxorGui::dataStore->started_id >= 0 || (startup_tun_pending && startup_deferred_profile_id >= 0)) {
        vpnArgs += Int2String(ProxorGui::dataStore->inbound_socks_port);
    }
    vpn_process->start(Linux_PkexecPath(), vpnArgs);
#endif
    if (!vpn_process->waitForStarted()) {
        vpn_process->deleteLater();
        if (startup_tun_pending) failStartupTunAuthorization();
        return false;
    }
    auto startupMarkerBuffer = std::make_shared<QString>();
    auto handleStandardOutput = [this, vpn_process, startupMarkerBuffer] {
        auto output = QString::fromUtf8(vpn_process->readAllStandardOutput());
        if (startup_tun_pending) {
            *startupMarkerBuffer += output;
            if (startupMarkerBuffer->contains("PROXOR_TUN_AUTHORIZED")) authorizeStartupTun();
            if (startupMarkerBuffer->contains("PROXOR_TUN_READY")) completeStartupTunAuthorization();
            if (startupMarkerBuffer->size() > 64) *startupMarkerBuffer = startupMarkerBuffer->right(64);
        }
        const auto log = output.replace("PROXOR_TUN_AUTHORIZED", "").replace("PROXOR_TUN_READY", "").trimmed();
        if (!log.isEmpty()) MW_show_log(log);
    };
    connect(vpn_process, &QProcess::readyReadStandardOutput, this, handleStandardOutput);
    connect(vpn_process, &QProcess::readyReadStandardError, this, [vpn_process] {
        const auto log = QString::fromUtf8(vpn_process->readAllStandardError()).trimmed();
        if (!log.isEmpty()) MW_show_log(log);
    });
    handleStandardOutput();
    vpn_pid = vpn_process->processId(); // actually it's pkexec or bash PID
#endif
    return true;
}

void MainWindow::resumeDeferredStartupProfile() {
    if (startup_deferred_profile_id >= 0) {
        const auto profileId = startup_deferred_profile_id;
        startup_deferred_profile_id = -1;
        if (ProxorGui::dataStore->core_running) {
            proxor_start(profileId);
        } else if (core_process != nullptr) {
            core_process->start_profile_when_core_is_up = profileId;
        } else {
            startup_deferred_profile_id = profileId;
        }
    }
}

#ifdef Q_OS_MACOS
void MainWindow::macTunFailed(const QString &reason) {
    mac_tun_failure_reason = reason.isEmpty() ? tr("unknown error") : reason;
    vpn_pid = 0;
    if (mac_tun_ready_timer) mac_tun_ready_timer->stop();
    if (ProxorGui::dataStore->started_id >= 0) {
        // The deferred profile was already resumed: stop it, no profile runs until Tun works or Tun is turned off.
        // remember_id must keep naming it (see proxor_stop stage2), so a quit or crash while blocked restores it.
        startup_deferred_profile_id = ProxorGui::dataStore->started_id;
        mac_stop_keeps_remembered_profile = true;
        proxor_stop();
    }
    failStartupTunAuthorization();
    // The check box shows Tun on so the user can switch it off: that is the escape from the block.
    ProxorGui::dataStore->spmode_vpn = true;
    refresh_status();
    const auto text = MacTunFailureText(mac_tun_failure_reason);
    MW_show_log(text);
    MessageBoxWarning(software_name, text);
}

void MainWindow::macStartupRestore(bool rememberedSystemProxy, bool rememberedTun) {
    if (!mac_startup_probe_pending) return; // nothing remembered: nothing to restore
    MacHelperSvc()->probe(this, 1000, [this, rememberedTun, rememberedSystemProxy](const MacHelperProbe &, MacHelperState st) {
        macStartupProbed(st, rememberedTun, rememberedSystemProxy);
    });
    // Safety net: a probe that never answers must not hold the remembered profile forever.
    if (!mac_startup_probe_timer) {
        mac_startup_probe_timer = new QTimer(this);
        mac_startup_probe_timer->setSingleShot(true);
        connect(mac_startup_probe_timer, &QTimer::timeout, this, [this, rememberedTun, rememberedSystemProxy] {
            macStartupProbed(MacHelperState::InstalledNotRunning, rememberedTun, rememberedSystemProxy);
        });
    }
    mac_startup_probe_timer->start(8000);
}

void MainWindow::macStartupProbed(MacHelperState st, bool rememberedTun, bool rememberedSystemProxy) {
    if (!mac_startup_probe_pending) return; // runs at most once (probe answer or safety timer)
    mac_startup_probe_pending = false;
    if (mac_startup_probe_timer) mac_startup_probe_timer->stop();
    const auto d = DecideMacTunStartup(rememberedTun, st);
    if (st == MacHelperState::Ready) {
        mac_spmode_restoring = true;
        if (rememberedSystemProxy) {
            proxor_set_spmode_system_proxy(true, false);
        }
        if (rememberedTun && d.setStartupTunPending) {
            proxor_set_spmode_vpn(true, false);
            if (ProxorGui::UseInternalTun() && ProxorGui::dataStore->spmode_vpn) {
                completeStartupTunAuthorization();
            } else if (!ProxorGui::UseInternalTun() && !ProxorGui::dataStore->spmode_vpn) {
                failStartupTunAuthorization();
            }
        }
        mac_spmode_restoring = false;
    } else {
        // Not Ready: never block. First release the startup hold so the profile connects in plain proxy mode
        // before any dialog appears (a hidden or ignored dialog, e.g. a tray start at login, can never deadlock).
        const auto inst = DecideMacStartupInstall(rememberedTun, rememberedSystemProxy, st, mac_install_prompted_this_session);
        if (startup_tun_pending) {
            // Release the hold without deadlock: the same steps as the "should be impossible" branch of proxor_set_spmode_vpn.
            startup_tun_pending = false;
            startup_tun_authorized = false;
            applyOnDemandAfterStartup();
            if (!ProxorGui::dataStore->prepare_exit) resumeDeferredStartupProfile();
            if (startup_network_work) {
                auto w = std::move(startup_network_work);
                w();
            }
        }
        if (inst.action == MacStartupInstallAction::Prompt && !ProxorGui::dataStore->prepare_exit) {
            mac_install_prompted_this_session = true; // at most one automatic prompt per session
            if (MacHelperInstaller::InstallInProgress()) {
                // The user already opened an installer (toggle or Tun settings): no second dialog.
                ProxorGui_log::WriteDiagnostic(tr("The Proxor service installation is already waiting for your answer."));
                return;
            }
            MW_show_log(tr("%1 is on, but the Proxor service is not installed or needs an update; asking to install it. Connected without it meanwhile.").arg(inst.feature));
            // Parentless when the window is hidden, so the dialog is not a sheet on an invisible window.
            MacHelperInstaller::ConfirmAndInstall(isVisible() ? this : nullptr, inst.feature, inst.enableAction,
                                                  [this, rememberedTun, rememberedSystemProxy](MacAdminScriptResult result) {
                if (result.outcome == MacAdminScriptOutcome::Ok) {
                    MW_show_log(tr("Proxor service installed."));
                    // The service is Ready now: the profile already runs, so Tun comes up on it without a restart.
                    const bool before = mac_spmode_restoring;
                    mac_spmode_restoring = true;
                    if (rememberedSystemProxy) proxor_set_spmode_system_proxy(true, false);
                    if (rememberedTun) proxor_set_spmode_vpn(true, false);
                    mac_spmode_restoring = before;
                    return;
                }
                // Cancelled or Failed: the profile stays connected without Tun; remember_spmode is untouched,
                // so the next launch (for example after a brew upgrade) asks again.
                auto text = MacStartupInstallDeclinedText(rememberedTun, rememberedSystemProxy);
                if (result.outcome == MacAdminScriptOutcome::Failed) {
                    const auto failure = tr("The Proxor service could not be installed: %1").arg(result.reason);
                    MW_show_log(failure);
                    text = failure + QStringLiteral("\n\n") + text;
                }
                MW_show_log(MacStartupInstallDeclinedText(rememberedTun, rememberedSystemProxy));
                refresh_status();
                if (result.outcome == MacAdminScriptOutcome::Failed) MessageBoxWarning(software_name, text);
            });
            return;
        }
        // LogOnly (or nothing to ask): today's behavior.
        if (!inst.logLine.isEmpty()) MW_show_log(inst.logLine);
        if (rememberedTun && rememberedSystemProxy) {
            MW_show_log(tr("System Proxy is remembered, but the Proxor service is not available; leaving it off. Turn on System Proxy to install the service."));
        }
    }
}

void MainWindow::macOnTunReady() {
    if (mac_tun_ready_timer) mac_tun_ready_timer->stop();
    mac_tun_failure_reason.clear();
    mac_stop_keeps_remembered_profile = false;
    MW_show_log(tr("Tun interface ready."));
    if (startup_tun_pending) completeStartupTunAuthorization();
}

void MainWindow::macOnTunStopped(const QString &reason) {
    if (startup_tun_pending) {
        macTunFailed(reason);
        return;
    }
    vpn_pid = 0;
    if (ProxorGui::dataStore->spmode_vpn && !ProxorGui::dataStore->prepare_exit) {
        MW_show_log(MacTunFailureText(reason));
        proxor_set_spmode_vpn(false, false); // not the user's choice: Tun stays remembered for the next launch
    }
}

void MainWindow::macOnHelperLost() {
    // The service's lease cleanup already restored the proxy and stopped Tun: forget any pending pause/park.
    mac_modes->reset();
    if (vpn_pid != 0 || startup_tun_pending) macOnTunStopped(tr("the Proxor service stopped"));
    if (ProxorGui::dataStore->spmode_system_proxy) {
        ProxorGui::dataStore->spmode_system_proxy = false;
        refresh_status();
        MW_show_log(tr("System Proxy turned off: the Proxor service stopped and restored your previous proxy settings."));
    }
}

void MainWindow::macApplySystemProxy(bool interactive, bool saved) {
    MacHelperSvc()->sysproxyApply(this, ProxorGui::dataStore->inbound_socks_port,
                                   ProxorPlatform::ReplaceBlanketPrivateRanges(MacDefaultProxyBypass(), ProxorPlatform::CurrentPrivateNetworks()), 20000,
                                  [this, interactive, saved](const MacHelperService::Reply &r) {
        if (!r.ok) {
            if (interactive) {
                MessageBoxWarning(software_name, tr("System Proxy could not be configured: %1").arg(r.error));
                // The switch was shown on optimistically: revert it (unless the user already turned it off).
                if (ProxorGui::dataStore->spmode_system_proxy) {
                    ProxorGui::dataStore->spmode_system_proxy = false;
                    if (saved) {
                        ProxorGui::dataStore->remember_spmode.removeAll("system_proxy");
                        ProxorGui::dataStore->Save();
                    }
                    refresh_status();
                }
            } else {
                MW_show_log(tr("[Warning] System Proxy could not be re-applied: %1").arg(r.error));
            }
            return;
        }
        for (const auto &f: r.body.value("failed").toArray()) {
            MW_show_log(tr("[Warning] System Proxy: %1").arg(f.toString()));
        }
        QStringList applied;
        for (const auto &a: r.body.value("applied").toArray()) applied << a.toString();
        MW_show_log(tr("System Proxy set on: %1").arg(applied.join(", ")));
    });
}

void MainWindow::macPauseModes(bool systemProxy, bool tun) {
    // Only the helper state changes here: both switches stay checked and remembered.
    // Tun goes first: the helper answers requests in order, and the System Proxy restore
    // takes seconds on a Mac with several network services. Until Tun is down every
    // connection still goes to the stopped profile's port.
    if (tun) {
        vpn_pid = 0;
        if (mac_tun_ready_timer) mac_tun_ready_timer->stop();
        MacHelperSvc()->tunStop(this, 5000, [this](const MacHelperService::Reply &r) {
            if (r.ok) {
                MW_show_log(tr("Tun paused: no profile is running; it resumes when a profile starts."));
            } else {
                MW_show_log(tr("[Warning] Tun stop: %1").arg(r.error));
            }
        });
    }
    if (systemProxy && (MacHelperSvc()->isConnected() || MacHelperSvc()->lastState() == MacHelperState::Ready)) {
        MacHelperSvc()->sysproxyRestore(this, 20000, [this](const MacHelperService::Reply &r) {
            if (r.ok) {
                MW_show_log(tr("System Proxy paused: your previous network proxy settings are back while no profile is running."));
            } else {
                MW_show_log(tr("[Warning] System Proxy restore: %1").arg(r.error));
            }
        });
    }
}

void MainWindow::macResumeModes(bool systemProxy, bool tun) {
    // Tun first, for the same reason as in macPauseModes: it is quick, the proxy is not.
    if (tun) {
        ProxorGui_log::WriteDiagnostic(tr("Proxy profile ready; starting Tun."));
        mac_tun_request_saves = false; // a resume must never un-remember Tun
        StartVPNProcess();
    }
    if (systemProxy) macApplySystemProxy(false);
}
#endif

void MainWindow::authorizeStartupTun() {
    if (!startup_tun_pending || startup_tun_authorized) return;
    startup_tun_authorized = true;
    MW_show_log(tr("Tun authorization granted; starting the deferred proxy profile."));
    resumeDeferredStartupProfile();
}

void MainWindow::completeStartupTunAuthorization() {
    if (!startup_tun_pending) return;
    startup_tun_pending = false;
    startup_tun_authorized = true;
    applyOnDemandAfterStartup();
    MW_show_log(tr("Tun interface ready; resuming deferred startup work."));

    resumeDeferredStartupProfile();
    if (startup_network_work) {
        auto startupWork = std::move(startup_network_work);
        startupWork();
    }
}

void MainWindow::failStartupTunAuthorization() {
    if (!startup_tun_pending) return;
    startup_tun_pending = false;
    startup_tun_authorized = false;
    startup_tun_failed = true;
    // startup_network_work is deliberately left in place: the work is not cancelled, only
    // held, so enabling Tun later -- or turning the mode off -- still runs it instead of
    // sending those requests outside the tunnel the user asked for.
    MW_show_log(tr("Tun authorization failed; startup network work waits until Tun is available."));
}

bool MainWindow::StopVPNProcess(bool unconditional) {
#ifdef Q_OS_MACOS
    {
        if (unconditional || vpn_pid != 0 || MacHelperSvc()->isConnected()) {
            MacHelperSvc()->tunStop(this, 5000, [this](const MacHelperService::Reply &r) {
                if (!r.ok && r.error != "timeout" && MacHelperSvc()->isConnected()) MW_show_log(tr("[Warning] Tun stop: %1").arg(r.error));
            });
        }
        vpn_pid = 0;
        if (mac_tun_ready_timer) mac_tun_ready_timer->stop();
        return true; // stopping never asks for a password and never fails the toggle, so the exit retry loop terminates
    }
#endif
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
#ifndef Q_OS_MACOS
        if (unconditional) {
            p.start(Linux_PkexecPath(), {"killall", "-2", "proxor_core"});
        } else {
            p.start(Linux_PkexecPath(), {"pkill", "-2", "-P", Int2String(vpn_pid)});
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
