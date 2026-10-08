#pragma once

#include <QMainWindow>

#include "main/ProxorGui.hpp"

#ifndef MW_INTERFACE

#include <QTime>
#include <QSet>
#include <QModelIndex>
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
#include <functional>
#include <memory>
#include <utility>

#include "GroupSort.hpp"
#include "platform/QrScanPolicy.hpp"
#include <QImage>

#include "sys/WifiMonitor.hpp"

#include "db/ProxyEntity.hpp"
#include "db/Group.hpp"
#include "main/GuiUtils.hpp"
#include "ui/model/ProxyListModel.h"
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
#include <QTimer>
#include "platform/WakeCoordinator.hpp"
#include "platform/WakeSubscriptionRetry.hpp"
#endif

#endif

namespace ProxorGui_sys {
    class CoreProcess;
}

#ifdef Q_OS_MACOS
namespace ProxorMac {
    class StatusItem;
}
#include "sys/macos/MacHelperPolicy.h"
#include "sys/macos/MacModeCoordinator.h"
#include "platform/MacAppUpdatePolicy.hpp"
#endif

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
class QLabel;
#ifdef Q_OS_MACOS
class QMenu;
class QTimer;
#endif
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
    // Live proxy speed next to the macOS menu bar icon (no-op elsewhere); clear=true removes it.
    void update_tray_speed(qint64 uploadBytesPerSecond, qint64 downloadBytesPerSecond, bool clear = false);

    void proxor_start(int _id = -1, bool startedByWifiTrigger = false);

    void proxor_stop(bool crash = false, bool sem = false);

    void proxor_set_spmode_system_proxy(bool enable, bool save = true);

    void proxor_set_spmode_vpn(bool enable, bool save = true);

    void show_log_impl(const QString &log);

    void start_select_mode(QObject *context, const std::function<void(int)> &callback);

    void refresh_connection_list(const QJsonArray &arr);

    [[nodiscard]] bool should_refresh_connection_statistics() const;

    void update_quota_display();

    void openSettings(const QString &section = QString());

    QStringList RegisterHotkey(bool unregister);

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

    void on_menu_about_triggered();

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

    void on_menu_scan_qr_image_triggered();

    void on_menu_scan_qr_clipboard_triggered();

    void importQrFromImage(const QImage &image, ProxorPlatform::QrSource source);

    void on_menu_clear_test_result_triggered();

    void on_menu_manage_groups_triggered();

    void on_menu_select_all_triggered();

    void on_menu_delete_repeat_triggered();

    void on_menu_remove_unavailable_triggered();

    void on_menu_update_subscription_triggered();
    
    void on_menu_add_subscription_triggered();

    void on_menu_resolve_domain_triggered();

    void on_proxyListTable_doubleClicked(const QModelIndex &index);

    void on_proxyListTable_customContextMenuRequested(const QPoint &pos);

    void on_toolButton_toggle_proxy_clicked();

    void on_tabWidget_currentChanged(int index);

    void on_down_tab_currentChanged(int index);

    void onWifiSsidChanged(const QString &ssid);

    void onWifiReadingChanged(const ProxorWifi::WifiReading &reading);

private:
    Ui::MainWindow *ui;
    QSystemTrayIcon *tray;
#ifdef Q_OS_MACOS
    ProxorMac::StatusItem *mac_status_item = nullptr;
    QMenu *mac_tray_menu = nullptr;
    QString mac_tun_failure_reason;
    bool mac_spmode_restoring = false;
    bool mac_screen_capture_requested = false;
    bool macScreenCaptureReadyOrExplain();
    QTimer *mac_tun_ready_timer = nullptr;
    bool mac_stop_keeps_remembered_profile = false;
    void macInstallHelperThen(const QString &feature, MacHelperEnableAction action, std::function<void()> onReady);
    void macTunFailed(const QString &reason);
    void macOnTunReady();
    void macOnTunStopped(const QString &reason);
    void macOnHelperLost();
    bool mac_startup_probe_pending = false;
    bool mac_install_prompted_this_session = false; // the automatic launch prompt happens at most once
    bool mac_tun_request_saves = true;              // whether a failed Tun start may un-remember Tun
    QTimer *mac_startup_probe_timer = nullptr;
    void macStartupRestore(bool rememberedSystemProxy, bool rememberedTun);
    void macStartupProbed(MacHelperState st, bool rememberedTun, bool rememberedSystemProxy);
    MacModeCoordinator *mac_modes = nullptr;
    void macApplySystemProxy(bool interactive, bool saved = false);
    void macPauseModes(bool systemProxy, bool tun);
    void macResumeModes(bool systemProxy, bool tun);
    void macWakeCheckModes(ProxorPlatform::WakeModes before);
    ProxorPlatform::MacAppUpdateRoute macAppUpdateRoute(PackageMode mode) const;
    void macAppUpdateFailed(bool duringDownload, const QString &error, const QString &releaseUrl);
    void macAppUpdateStaged();
    void macAppUpdateShowResult();
    ProxorPlatform::MacAppUpdateRoute mac_app_update_route = ProxorPlatform::MacAppUpdateRoute::Guidance;
    QString mac_app_update_zip_dir;
    QString mac_app_update_release_url;
    QStringList mac_app_update_args;
#endif
    QShortcut *shortcut_ctrl_f = new QShortcut(QKeySequence("Ctrl+F"), this);
    QShortcut *shortcut_ctrl_v = new QShortcut(QKeySequence("Ctrl+V"), this);
    QShortcut *shortcut_ctrl_a = new QShortcut(QKeySequence("Ctrl+A"), this);
    QShortcut *shortcut_ctrl_c = new QShortcut(QKeySequence("Ctrl+C"), this);
    QShortcut *shortcut_ctrl_s = new QShortcut(QKeySequence("Ctrl+S"), this);
    QShortcut *shortcut_esc = new QShortcut(QKeySequence("Esc"), this);
    //
    ProxorGui_sys::CoreProcess *core_process = nullptr;
    WifiMonitor *wifi_monitor = nullptr;
    bool wifi_permission_asked = false;
    bool wifi_settings_hint_logged = false;
    QString wifi_hosts_ssid;
    QString wifi_last_logged_status;
    void refreshWifiMonitoring();
    bool applyOnDemandForSsid(const QString &ssid);
    void applyOnDemandAfterStartup();
    qint64 vpn_pid = 0;
    //
    bool update_staged = false;
    QString staged_asset_name; // base name of the asset requested for Download, so
                                // onUpdateStaged() can derive the AppImage staged path
                                // without a second RPC
    QPointer<UpdateProgressDialog> updateProgressDialog;
#ifdef Q_OS_WIN
    QString update_release_url; // release page of the update being downloaded, for the failure dialog (phase 57)
#endif
    ProxyListModel *proxyListModel = nullptr;
    QLabel *m_quotaLabel = nullptr;
    //
    bool qvLogAutoScoll = true;
    QTextDocument *qvLogDocument = new QTextDocument(this);
    QVector<QString> m_logLines; // raw lines fed to show_log_impl, for filter re-render
    //
    QString title_error;
    int icon_status = -1;
    std::shared_ptr<ProxorGui::ProxyEntity> running;
    bool start_pending = false;
    QAction *tray_toggle_action = nullptr; // tray menu: Connect / Disconnect
    int last_started_profile_id = -1;       // Connect with nothing marked reconnects this profile
    std::atomic_bool start_cancel{false}; // Stop pressed while a start is in flight
    bool started_via_ssid_trigger = false;
    bool startup_tun_pending = false;
    bool startup_tun_authorized = false;
    bool startup_tun_failed = false;
    int startup_deferred_profile_id = -1;
    std::function<void()> startup_network_work;
    bool application_was_inactive = false;
    bool subscription_resume_check_pending = false;
    qint64 subscription_timer_last_tick_ms = 0;
#if defined(Q_OS_MACOS) || defined(Q_OS_LINUX)
    // Sleep/wake resilience (phase 56): native events + the timer gap feed one coordinator. Windows keeps the timer heuristic only.
    ProxorPlatform::WakeCoordinator wake_coord;
    ProxorPlatform::WakeSubscriptionRetry wake_subs;
    QTimer *wake_timer = nullptr;
    QTimer *wake_subs_timer = nullptr;
    void wakeInstall();
    void wakeOnSleepEvent(bool sleeping);
    void wakeDetected(ProxorPlatform::WakeSource source);
    void wakeRunStep();
    void wakeSubsStep();
    bool wakeOwnsSubscriptions() const;
    bool wakeBlocked() const;
    ProxorPlatform::WakeSnapshot wakeSnapshotNow() const;
#endif
    QString auto_start_consumed_ssid;
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
    [[nodiscard]] std::shared_ptr<ProxorGui::ProxyEntity> resolveSsidOnDemandProfile() const;

    void dialog_message_impl(const QString &sender, const QString &info);

    void refresh_proxy_list_impl(const int &id = -1, GroupSortAction groupSortAction = {});

    void refresh_proxy_list_impl_refresh_data(const int &id = -1);
    
    void refresh_proxy_list_impl_refresh_data(const QSet<int> &ids);

    void apply_proxy_list_search(const QString &text);

    void keyPressEvent(QKeyEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

    void changeEvent(QEvent *event) override;

    void showEvent(QShowEvent *event) override;

    void hideEvent(QHideEvent *event) override;

    //

    void HotkeyEvent(const QString &key);

    bool StartVPNProcess();

    void authorizeStartupTun();
    void completeStartupTunAuthorization();
    void failStartupTunAuthorization();
    void resumeDeferredStartupProfile();

    void syncWindowsHostsMapping(bool enable);

    void update_connection_statistics_polling_state();
    void queue_resume_subscription_check();

    // grpc and ...

    static void setup_grpc();

    void speedtest_current_group(int mode, bool test_group);

    void speedtest_profiles(const QList<std::shared_ptr<ProxorGui::ProxyEntity>> &profiles, int mode, bool groupedLogs, bool logFailuresOnly = false, bool silent = false);

    void speedtest_current();

    void run_subscription_ping_on_open(int attempts = 0);

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
