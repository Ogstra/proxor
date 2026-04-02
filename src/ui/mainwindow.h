#pragma once

#include <QMainWindow>

#include "main/ProxorGui.hpp"

#ifndef MW_INTERFACE

#include <QTime>
#include <QTableWidgetItem>
#include <QSet>
#include <QKeyEvent>
#include <QSystemTrayIcon>
#include <QPointer>
#include "dialog_update_progress.h"
#include <QProcess>
#include <QTextDocument>
#include <QShortcut>
#include <QSemaphore>
#include <QMutex>
#include <atomic>

#include "GroupSort.hpp"

#include "sys/WifiMonitor.hpp"

#include "db/ProxyEntity.hpp"
#include "db/Group.hpp"
#include "main/GuiUtils.hpp"

#endif

namespace ProxorGui_sys {
    class CoreProcess;
}

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

    void refresh_proxy_list(const int &id = -1);
    
    void refresh_proxy_list_rows(const QList<int> &ids);

    void show_group(int gid);

    void refresh_groups();

    void refresh_status(const QString &traffic_update = "");

    void proxor_start(int _id = -1);

    void proxor_stop(bool crash = false, bool sem = false);

    void proxor_set_spmode_system_proxy(bool enable, bool save = true);

    void proxor_set_spmode_vpn(bool enable, bool save = true);

    void show_log_impl(const QString &log);

    void start_select_mode(QObject *context, const std::function<void(int)> &callback);

    void refresh_connection_list(const QJsonArray &arr);

    [[nodiscard]] bool should_refresh_connection_statistics() const;

    void RegisterHotkey(bool unregister);

    bool StopVPNProcess(bool unconditional = false);

signals:

    void profile_selected(int id);

public slots:

    void on_commitDataRequest();

    void on_menu_exit_triggered();
    
    void onUpdateStaged();

#ifndef MW_INTERFACE

private slots:

    void on_masterLogBrowser_customContextMenuRequested(const QPoint &pos);

    void on_menu_basic_settings_triggered();

    void on_menu_routing_settings_triggered();

    void on_menu_vpn_settings_triggered();

    void on_menu_ssid_settings_triggered();

    void on_menu_hotkey_settings_triggered();

    void on_menu_add_from_input_triggered();

    void on_menu_add_from_clipboard_triggered();

    void on_menu_clone_triggered();

    void on_menu_move_triggered();

    void on_menu_delete_triggered();

    void on_menu_reset_traffic_triggered();

    void on_menu_profile_debug_info_triggered();

    void on_menu_copy_links_triggered();

    void on_menu_copy_links_nkr_triggered();

    void on_menu_export_config_triggered();

    void display_qr_link(bool nkrFormat = false);

    void on_menu_scan_qr_triggered();

    void on_menu_clear_test_result_triggered();

    void on_menu_manage_groups_triggered();

    void on_menu_select_all_triggered();

    void on_menu_delete_repeat_triggered();

    void on_menu_remove_unavailable_triggered();

    void on_menu_update_subscription_triggered();
    
    void on_menu_add_subscription_triggered();

    void on_menu_resolve_domain_triggered();

    void on_proxyListTable_itemDoubleClicked(QTableWidgetItem *item);

    void on_proxyListTable_itemChanged(QTableWidgetItem *item);

    void on_proxyListTable_customContextMenuRequested(const QPoint &pos);

    void on_toolButton_toggle_proxy_clicked();

    void on_tabWidget_currentChanged(int index);

    void on_down_tab_currentChanged(int index);

    void onWifiSsidChanged(const QString &ssid);

private:
    Ui::MainWindow *ui;
    QSystemTrayIcon *tray;
    QShortcut *shortcut_ctrl_f = new QShortcut(QKeySequence("Ctrl+F"), this);
    QShortcut *shortcut_esc = new QShortcut(QKeySequence("Esc"), this);
    //
    ProxorGui_sys::CoreProcess *core_process;
    WifiMonitor *wifi_monitor = nullptr;
    qint64 vpn_pid = 0;
    //
    bool update_staged = false;
    QPointer<UpdateProgressDialog> updateProgressDialog;
    //
    bool qvLogAutoScoll = true;
    QTextDocument *qvLogDocument = new QTextDocument(this);
    QVector<QString> m_logLines; // raw lines fed to show_log_impl, for filter re-render
    //
    QString title_error;
    int icon_status = -1;
    std::shared_ptr<ProxorGui::ProxyEntity> running;
    bool start_pending = false;
    QString traffic_update_cache;
    QTime last_test_time;
    //
    int proxy_last_order = -1;
    bool select_mode = false;
    QMutex mu_starting;
    QMutex mu_stopping;
    QMutex mu_exit;
    QSemaphore sem_stopped;
    int exit_reason = 0;
    std::atomic_bool conn_stats_tab_active{false};
    std::atomic_bool conn_stats_window_visible{false};

    void rebuildLogDocument(const QString &filter);

    QList<std::shared_ptr<ProxorGui::ProxyEntity>> get_now_selected_list();

    QList<std::shared_ptr<ProxorGui::ProxyEntity>> get_selected_or_group();

    QList<int> get_toggle_proxy_ids(const std::shared_ptr<ProxorGui::Group> &group) const;

    void dialog_message_impl(const QString &sender, const QString &info);

    void refresh_proxy_list_impl(const int &id = -1, GroupSortAction groupSortAction = {});

    void refresh_proxy_list_impl_refresh_data(const int &id = -1);
    
    void refresh_proxy_list_impl_refresh_data(const QSet<int> &ids);

    void keyPressEvent(QKeyEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

    void changeEvent(QEvent *event) override;

    void showEvent(QShowEvent *event) override;

    void hideEvent(QHideEvent *event) override;

    //

    void HotkeyEvent(const QString &key);

    bool StartVPNProcess();

    void update_connection_statistics_polling_state();

    // grpc and ...

    static void setup_grpc();

    void speedtest_current_group(int mode, bool test_group);

    void speedtest_current();

    static void stop_core_daemon();

    void CheckUpdate(bool silent = false);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

#endif // MW_INTERFACE
};

inline MainWindow *GetMainWindow() {
    return (MainWindow *) mainwindow;
}

void UI_InitMainWindow();
