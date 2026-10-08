// MainWindow glue for sleep/wake resilience (phase 56). Compiled on macOS and Linux only (per-OS CMake lists);
// Windows keeps the subscription-timer heuristic and links none of this.
#include "sys/LogFile.hpp"
#include "mainwindow.h"
#include "sys/SleepWake.hpp"
#include "sub/GroupUpdater.hpp"
#include "db/Database.hpp"
#include "rpc/gRPC.h"

#include <QDateTime>
#include <QNetworkInformation>
#include <QThreadPool>
#include <QTimer>

#ifdef Q_OS_MACOS
#include "sys/macos/MacHelperService.h"
#include "sys/macos/MacHelperClient.h"
#endif

void MainWindow::wakeInstall() {
    wake_timer = new QTimer(this);
    wake_timer->setSingleShot(true);
    connect(wake_timer, &QTimer::timeout, this, [this] { wakeRunStep(); });
    wake_subs_timer = new QTimer(this);
    wake_subs_timer->setSingleShot(true);
    connect(wake_subs_timer, &QTimer::timeout, this, [this] { wakeSubsStep(); });

    const auto r = ProxorSleepWake::Install(this, [this](bool sleeping) { wakeOnSleepEvent(sleeping); });
    ProxorGui_log::WriteDiagnostic("[Wake] " + r.detail);
}

bool MainWindow::wakeBlocked() const {
    return startup_tun_pending || startup_tun_failed || ProxorGui::dataStore->prepare_exit;
}

bool MainWindow::wakeOwnsSubscriptions() const {
    return wake_coord.active() || wake_subs.active();
}

ProxorPlatform::WakeSnapshot MainWindow::wakeSnapshotNow() const {
    ProxorPlatform::WakeSnapshot s;
    s.profileId = ProxorGui::dataStore->started_id >= 0 ? ProxorGui::dataStore->started_id : -1;
#ifdef Q_OS_MACOS
    s.modes.systemProxy = ProxorGui::dataStore->spmode_system_proxy && !(mac_modes && mac_modes->systemProxyParked());
    s.modes.tun = ProxorGui::dataStore->spmode_vpn && vpn_pid != 0;
#endif
    return s;
}

void MainWindow::wakeOnSleepEvent(bool sleeping) {
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (!sleeping) {
        wakeDetected(ProxorPlatform::WakeSource::Native);
        return;
    }
    if (wake_timer) wake_timer->stop();
    if (wake_subs_timer) wake_subs_timer->stop();
    wake_subs.cancel();
    const auto snap = wakeSnapshotNow();
    wake_coord.noteSleep(now, snap);
    ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] going to sleep: profile %1, System Proxy %2, Tun %3")
                                       .arg(snap.profileId)
                                       .arg(snap.modes.systemProxy ? "on" : "off")
                                       .arg(snap.modes.tun ? "on" : "off"));
}

void MainWindow::wakeDetected(ProxorPlatform::WakeSource source) {
    if (ProxorGui::dataStore->prepare_exit) return;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    const bool native = source == ProxorPlatform::WakeSource::Native;
    const QString sourceName = native ? QStringLiteral("native") : QStringLiteral("timer gap");
    if (!wake_coord.noteWake(source, now, wakeSnapshotNow())) {
        ProxorGui_log::WriteDiagnostic("[Wake] duplicate wake signal ignored (" + sourceName + ")");
        return;
    }
    if (wake_subs_timer) wake_subs_timer->stop();
    wake_subs.cancel();
    ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] resumed from sleep (%1); restoring profile %2 if needed")
                                       .arg(sourceName)
                                       .arg(wake_coord.snapshot().profileId));
    if (wake_timer) wake_timer->start(0);
    wakeResetCoreNetwork();
}

// With Tun on, every app's traffic goes through the core, so connections that were open before the sleep stay
// dead until the core drops them. Done off the GUI thread: the call waits for the core's reply.
void MainWindow::wakeResetCoreNetwork() {
    if (!ProxorGui::dataStore->spmode_vpn || !ProxorGui::dataStore->core_running) return;
    QTimer::singleShot(1500, this, [this] {
        if (ProxorGui::dataStore->prepare_exit || !ProxorGui::dataStore->core_running) return;
        QThreadPool::globalInstance()->start([this] {
            const bool ok = ProxorGui_rpc::defaultClient->ResetNetwork();
            QMetaObject::invokeMethod(this, [ok] {
                ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] core network reset: %1").arg(ok ? "ok" : "failed"));
            }, Qt::QueuedConnection);
        });
    });
}

void MainWindow::wakeRunStep() {
    using namespace ProxorPlatform;
    const auto now = QDateTime::currentMSecsSinceEpoch();

    WakeObservation o;
    o.nowMs = now;
    auto *ni = QNetworkInformation::instance();
    if (ni == nullptr || ni->reachability() == QNetworkInformation::Reachability::Unknown) {
        o.reachability = WakeReachability::Unknown;
    } else if (ni->reachability() == QNetworkInformation::Reachability::Online) {
        o.reachability = WakeReachability::Online;
    } else {
        o.reachability = WakeReachability::Offline;
    }
    o.blocked = wakeBlocked();
    o.startPending = start_pending;
    o.coreRunning = ProxorGui::dataStore->core_running;
    o.runningProfileId = ProxorGui::dataStore->started_id >= 0 ? ProxorGui::dataStore->started_id : -1;

    const auto s = wake_coord.step(o);
    switch (s.action) {
        case WakeAction::Wait:
            wake_timer->start(s.delayMs);
            break;
        case WakeAction::RestartProfile: {
            auto ent = ProxorGui::profileManager->GetProfile(s.profileId);
            if (ent == nullptr) {
                ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] profile %1 no longer exists; not restarting it").arg(s.profileId));
            } else {
                MW_show_log(WakeCoreRestartLine(ent->bean->DisplayTypeAndName()));
                proxor_start(s.profileId);
            }
            if (s.delayMs >= 0) wake_timer->start(s.delayMs);
            break;
        }
        case WakeAction::CheckModes:
#ifdef Q_OS_MACOS
            macWakeCheckModes(s.modes);
#endif
            if (s.delayMs >= 0) wake_timer->start(s.delayMs);
            break;
        case WakeAction::UpdateSubscriptions:
            wake_subs.begin(now);
            wakeSubsStep();
            break;
        case WakeAction::Finished:
            switch (s.note) {
                case WakeNote::Blocked:
                    ProxorGui_log::WriteDiagnostic("[Wake] skipped: startup Tun pending or exiting");
                    break;
                case WakeNote::NetworkTimedOut:
                    ProxorGui_log::WriteDiagnostic("[Wake] the network was not reachable within 60 s; subscription updates wait for the regular schedule");
                    break;
                case WakeNote::RunTimedOut:
                    ProxorGui_log::WriteDiagnostic("[Wake] gave up waiting (a profile start stayed pending)");
                    break;
                default:
                    break;
            }
            break;
    }
}

void MainWindow::wakeSubsStep() {
    using namespace ProxorPlatform;
    SubscriptionRetryInputs in;
    in.nowMs = QDateTime::currentMSecsSinceEpoch();
    in.blocked = wakeBlocked();
    in.scheduled = UI_has_scheduled_subscription_updates();
    in.startPending = start_pending;
    in.updateRunning = UI_subscription_updates_running();
    in.anyDue = UI_has_due_subscription_updates();

    const auto s = wake_subs.step(in);
    switch (s.action) {
        case SubscriptionRetryAction::Wait:
            wake_subs_timer->start(s.delayMs);
            break;
        case SubscriptionRetryAction::StartUpdate:
            ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] subscription update attempt %1").arg(s.attempt));
            UI_update_due_groups_on_timer();
            wake_subs_timer->start(s.delayMs);
            break;
        case SubscriptionRetryAction::Done:
            if (wake_subs.attempts() > 0) {
                ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] subscriptions done after %1 attempt(s)").arg(wake_subs.attempts()));
            }
            break;
        case SubscriptionRetryAction::GaveUp:
            MW_show_log(WakeSubscriptionGaveUpLine(s.attempt));
            break;
    }
}

#ifdef Q_OS_MACOS
void MainWindow::macWakeCheckModes(ProxorPlatform::WakeModes before) {
    if (!(before.systemProxy || before.tun)) return;
    MacHelperSvc()->status(this, 3000, [this, before](const MacHelperService::Reply &r) {
        using namespace ProxorPlatform;
        if (ProxorGui::dataStore->prepare_exit) return;
        if (!r.ok) {
            MW_show_log(WakeModesCheckFailedLine(r.error)); // e.g. the service is gone: never prompt to install here
            return;
        }
        const WakeModes helper{r.body.value(MacHelperWire::kKeyProxyApplied).toBool(), r.body.value(MacHelperWire::kKeyTunRunning).toBool()};
        const WakeModes fix = DecideWakeModeRepair(before, helper, ProxorGui::dataStore->started_id >= 0,
                                                   mac_modes && mac_modes->systemProxyParked(), mac_modes && mac_modes->tunPaused());
        ProxorGui_log::WriteDiagnostic(QStringLiteral("[Wake] service status: System Proxy %1, Tun %2").arg(helper.systemProxy).arg(helper.tun));
        if (!fix.systemProxy && !fix.tun) return;
        if (!MacHelperSvc()->isConnected()) {
            MW_show_log(WakeModesCheckFailedLine(tr("the Proxor service is not connected")));
            return;
        }
        MW_show_log(WakeModesReappliedLine(fix));
        if (fix.tun) { // Tun first, like macResumeModes: quick, the proxy is not
            if (ProxorGui::dataStore->spmode_vpn) {
                mac_tun_request_saves = false;
                StartVPNProcess();
            } else {
                proxor_set_spmode_vpn(true, false); // turned off by macOnTunStopped while asleep; not saved
            }
        }
        if (fix.systemProxy) {
            if (ProxorGui::dataStore->spmode_system_proxy) {
                macApplySystemProxy(false);
            } else {
                proxor_set_spmode_system_proxy(true, false); // turned off by macOnHelperLost while asleep; not saved
            }
        }
    });
}
#endif
