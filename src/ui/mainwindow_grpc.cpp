#include "./ui_mainwindow.h"
#include "mainwindow.h"

#include "db/Database.hpp"
#include "db/ConfigBuilder.hpp"
#include "db/traffic/TrafficLooper.hpp"
#include "rpc/gRPC.h"
#include "ui/widget/MessageBoxTimer.h"

#include <QTimer>
#include <QThread>
#include <QInputDialog>
#include <QPushButton>
#include <QDesktopServices>
#include <QMessageBox>
#include <QDialogButtonBox>

// ext core

std::list<std::shared_ptr<ProxorGui_sys::ExternalProcess>> CreateExtCFromExtR(const std::list<std::shared_ptr<ProxorGui_fmt::ExternalBuildResult>> &extRs, bool start) {
    // plz run and start in same thread
    std::list<std::shared_ptr<ProxorGui_sys::ExternalProcess>> l;
    for (const auto &extR: extRs) {
        std::shared_ptr<ProxorGui_sys::ExternalProcess> extC(new ProxorGui_sys::ExternalProcess());
        extC->tag = extR->tag;
        extC->program = extR->program;
        extC->arguments = extR->arguments;
        extC->env = extR->env;
        l.emplace_back(extC);
        //
        if (start) extC->Start();
    }
    return l;
}

// grpc

#ifndef NKR_NO_GRPC
using namespace ProxorGui_rpc;
#endif

void MainWindow::setup_grpc() {
#ifndef NKR_NO_GRPC
    // Setup Connection
    defaultClient = new Client(
        [=](const QString &errStr) {
            MW_show_log("[Error] gRPC: " + errStr);
        },
        "127.0.0.1:" + Int2String(ProxorGui::dataStore->core_port), ProxorGui::dataStore->core_token);

    // Looper
    runOnNewThread([=] { ProxorGui_traffic::trafficLooper->Loop(); });
#endif
}

// 测速

inline bool speedtesting = false;
inline QList<QThread *> speedtesting_threads = {};

void MainWindow::speedtest_current_group(int mode, bool test_group) {
    if (speedtesting) {
        MessageBoxWarning(software_name, QObject::tr("The last speed test did not exit completely, please wait. If it persists, please restart the program."));
        return;
    }

    auto profiles = get_selected_or_group();
    if (test_group) profiles = ProxorGui::profileManager->CurrentGroup()->ProfilesWithOrder();
    if (profiles.isEmpty()) return;
    auto group = ProxorGui::profileManager->CurrentGroup();
    if (group->archive) return;

    // menu_stop_testing
    if (mode == 114514) {
        while (!speedtesting_threads.isEmpty()) {
            auto t = speedtesting_threads.takeFirst();
            if (t != nullptr) t->exit();
        }
        speedtesting = false;
        return;
    }

#ifndef NKR_NO_GRPC
    QStringList full_test_flags;
    if (mode == libcore::FullTest) {
        auto w = new QDialog(this);
        auto layout = new QVBoxLayout(w);
        w->setWindowTitle(tr("Test Options"));
        //
        auto l1 = new QCheckBox(tr("Latency"));
        auto l2 = new QCheckBox(tr("UDP latency"));
        auto l3 = new QCheckBox(tr("Download speed"));
        auto l4 = new QCheckBox(tr("In and Out IP"));
        //
        auto box = new QDialogButtonBox;
        box->setOrientation(Qt::Horizontal);
        box->setStandardButtons(QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
        connect(box, &QDialogButtonBox::accepted, w, &QDialog::accept);
        connect(box, &QDialogButtonBox::rejected, w, &QDialog::reject);
        //
        layout->addWidget(l1);
        layout->addWidget(l2);
        layout->addWidget(l3);
        layout->addWidget(l4);
        layout->addWidget(box);
        if (w->exec() != QDialog::Accepted) {
            w->deleteLater();
            return;
        }
        //
        if (l1->isChecked()) full_test_flags << "1";
        if (l2->isChecked()) full_test_flags << "2";
        if (l3->isChecked()) full_test_flags << "3";
        if (l4->isChecked()) full_test_flags << "4";
        //
        w->deleteLater();
        if (full_test_flags.isEmpty()) return;
    }
    speedtesting = true;

    runOnNewThread([this, profiles, mode, full_test_flags, test_group]() {
        QMutex lock_write;
        QMutex lock_return;
        QMutex lock_results;
        int threadN = ProxorGui::dataStore->test_concurrent;
        if (mode == libcore::FullTest && full_test_flags.contains("3")) {
            threadN = 1;
        }
        int threadN_finished = 0;
        auto profiles_test = profiles; // copy
        QStringList final_logs;
        QHash<int, int> profile_log_index;
        for (int i = 0; i < profiles.count(); i++) {
            final_logs << "";
            profile_log_index.insert(profiles[i]->id, i);
        }

        // Threads
        lock_return.lock();
        for (int i = 0; i < threadN; i++) {
            runOnNewThread([&] {
                speedtesting_threads << QObject::thread();

                forever {
                    //
                    lock_write.lock();
                    if (profiles_test.isEmpty()) {
                        threadN_finished++;
                        if (threadN == threadN_finished) {
                            // quit control thread
                            lock_return.unlock();
                        }
                        lock_write.unlock();
                        // quit of this thread
                        speedtesting_threads.removeAll(QObject::thread());
                        return;
                    }
                    auto profile = profiles_test.takeFirst();
                    lock_write.unlock();

                    //
                    libcore::TestReq req;
                    req.set_mode((libcore::TestMode) mode);
                    req.set_timeout(10 * 1000);
                    req.set_url(ProxorGui::dataStore->test_latency_url.toStdString());

                    //
                    std::list<std::shared_ptr<ProxorGui_sys::ExternalProcess>> extCs;
                    QSemaphore extSem;

                    if (mode == libcore::TestMode::UrlTest || mode == libcore::FullTest) {
                        auto c = BuildConfig(profile, true, false);
                        if (!c->error.isEmpty()) {
                            profile->full_test_report = c->error;
                            ProxorGui::profileManager->SaveProfile(profile);
                            if (test_group) {
                                lock_results.lock();
                                final_logs[profile_log_index.value(profile->id)] = tr("[%1] test error: %2").arg(profile->bean->DisplayTypeAndName(), c->error);
                                lock_results.unlock();
                            }
                            auto profileId = profile->id;
                            runOnUiThread([this, profileId] {
                                refresh_proxy_list(profileId);
                            });
                            continue;
                        }
                        //
                        if (!c->extRs.empty()) {
                            runOnUiThread(
                                [&] {
                                    extCs = CreateExtCFromExtR(c->extRs, true);
                                    QThread::msleep(500);
                                    extSem.release();
                                },
                                DS_cores);
                            extSem.acquire();
                        }
                        //
                        auto config = new libcore::LoadConfigReq;
                        config->set_core_config(QJsonObject2QString(c->coreConfig, false).toStdString());
                        req.set_allocated_config(config);
                        req.set_in_address(profile->bean->serverAddress.toStdString());

                        req.set_full_latency(full_test_flags.contains("1"));
                        req.set_full_udp_latency(full_test_flags.contains("2"));
                        req.set_full_speed(full_test_flags.contains("3"));
                        req.set_full_in_out(full_test_flags.contains("4"));

                        req.set_full_speed_url(ProxorGui::dataStore->test_download_url.toStdString());
                        req.set_full_speed_timeout(ProxorGui::dataStore->test_download_timeout);
                    } else if (mode == libcore::TcpPing) {
                        req.set_address(profile->bean->DisplayAddress().toStdString());
                    }

                    bool rpcOK;
                    auto result = defaultClient->Test(&rpcOK, req);
                    //
                    if (!extCs.empty()) {
                        runOnUiThread(
                            [&] {
                                for (const auto &extC: extCs) {
                                    extC->Kill();
                                }
                                extSem.release();
                            },
                            DS_cores);
                        extSem.acquire();
                    }
                    //
                    if (!rpcOK) {
                        if (test_group) {
                            lock_results.lock();
                            final_logs[profile_log_index.value(profile->id)] = tr("[%1] test error: RPC failed").arg(profile->bean->DisplayTypeAndName());
                            lock_results.unlock();
                        }
                        return;
                    }

                    if (result.error().empty()) {
                        profile->latency = result.ms();
                        if (profile->latency == 0) profile->latency = 1; // legacy format uses 0 to represent not tested
                    } else {
                        profile->latency = -1;
                    }
                    profile->full_test_report = result.full_report().c_str(); // higher priority
                    ProxorGui::profileManager->SaveProfile(profile);

                    QString result_log;
                    if (!result.error().empty()) {
                        result_log = tr("[%1] test error: %2").arg(profile->bean->DisplayTypeAndName(), result.error().c_str());
                    } else if (!result.full_report().empty()) {
                        result_log = tr("[%1] %2").arg(profile->bean->DisplayTypeAndName(), result.full_report().c_str());
                    }
                    if (test_group) {
                        lock_results.lock();
                        final_logs[profile_log_index.value(profile->id)] = result_log;
                        lock_results.unlock();
                    } else if (!result_log.isEmpty()) {
                        MW_show_log(result_log);
                    }

                    auto profileId = profile->id;
                    runOnUiThread([this, profileId] {
                        refresh_proxy_list(profileId);
                    });
                }
            });
        }

        // Control
        lock_return.lock();
        lock_return.unlock();
        speedtesting = false;
        if (test_group) {
            lock_results.lock();
            auto logs = final_logs;
            lock_results.unlock();
            logs.removeAll("");
            if (!logs.isEmpty()) {
                MW_show_log(logs.join("\n"));
            }
        }
        MW_show_log(QObject::tr("Speedtest finished."));
    });
#endif
}

void MainWindow::speedtest_current() {
#ifndef NKR_NO_GRPC
    last_test_time = QTime::currentTime();
    ui->label_running->setText(tr("Testing"));

    runOnNewThread([=] {
        libcore::TestReq req;
        req.set_mode(libcore::UrlTest);
        req.set_timeout(10 * 1000);
        req.set_url(ProxorGui::dataStore->test_latency_url.toStdString());

        bool rpcOK;
        auto result = defaultClient->Test(&rpcOK, req);
        if (!rpcOK) return;

        auto latency = result.ms();
        last_test_time = QTime::currentTime();

        runOnUiThread([=] {
            if (!result.error().empty()) {
                MW_show_log(QStringLiteral("UrlTest error: %1").arg(result.error().c_str()));
            }
            if (latency <= 0) {
                ui->label_running->setText(tr("Test Result") + ": " + tr("Unavailable"));
            } else if (latency > 0) {
                ui->label_running->setText(tr("Test Result") + ": " + QStringLiteral("%1 ms").arg(latency));
            }
        });
    });
#endif
}

void MainWindow::stop_core_daemon() {
#ifndef NKR_NO_GRPC
    ProxorGui_rpc::defaultClient->Exit();
#endif
}

void MainWindow::proxor_start(int _id) {
    if (ProxorGui::dataStore->prepare_exit) return;

    auto ents = get_now_selected_list();
    auto ent = (_id < 0 && !ents.isEmpty()) ? ents.first() : ProxorGui::profileManager->GetProfile(_id);
    if (ent == nullptr) return;

    if (select_mode) {
        emit profile_selected(ent->id);
        select_mode = false;
        refresh_status();
        return;
    }

    auto group = ProxorGui::profileManager->GetGroup(ent->gid);
    if (group == nullptr || group->archive) return;

    auto result = BuildConfig(ent, false, false);
    if (!result->error.isEmpty()) {
        start_pending = false;
        refresh_status();
        MessageBoxWarning("BuildConfig return error", result->error);
        return;
    }

    auto proxor_start_stage2 = [=] {
#ifndef NKR_NO_GRPC
        libcore::LoadConfigReq req;
        auto coreConfig = QJsonObject2QString(result->coreConfig, false);
        req.set_core_config(coreConfig.toStdString());
        req.set_enable_connection_statistics(ProxorGui::dataStore->connection_statistics);
        if (ProxorGui::dataStore->traffic_loop_interval > 0) {
            req.add_stats_outbounds("proxy");
            req.add_stats_outbounds("bypass");
        }
        //
        bool rpcOK;
        QString error;
        // Retry on port-bind errors: Windows releases sockets with a brief delay after Stop(),
        // so a fast restart can see "address already in use" on the first attempt.
        for (int attempt = 0; attempt <= 3; attempt++) {
            if (attempt > 0) QThread::msleep(200);
            error = defaultClient->Start(&rpcOK, req);
            if (!rpcOK || error.isEmpty()) break;       // RPC failure or success — don't retry
            if (!error.contains("bind:")) break;        // unrelated error — don't retry
        }
        if (rpcOK && !error.isEmpty()) {
            start_pending = false;
            runOnUiThread([=] {
                refresh_status();
                MessageBoxWarning("LoadConfig return error", error);
            });
            return false;
        } else if (!rpcOK) {
            start_pending = false;
            runOnUiThread([=] { refresh_status(); });
            return false;
        }
        //
        ProxorGui_traffic::trafficLooper->proxy = result->outboundStat.get();
        ProxorGui_traffic::trafficLooper->items = result->outboundStats;
        ProxorGui::dataStore->ignoreConnTag = result->ignoreConnTag;
        ProxorGui_traffic::trafficLooper->loop_enabled = true;
#endif

        runOnUiThread(
            [=] {
                auto extCs = CreateExtCFromExtR(result->extRs, true);
                ProxorGui_sys::running_ext.splice(ProxorGui_sys::running_ext.end(), extCs);
            },
            DS_cores);

        ProxorGui::dataStore->UpdateStartedId(ent->id);
        running = ent;

        runOnUiThread([=] {
            start_pending = false;
            refresh_status();
            refresh_proxy_list(ent->id);
        });

        return true;
    };

    if (!mu_starting.tryLock()) {
        return;
    }
    if (!mu_stopping.tryLock()) {
        start_pending = false;
        refresh_status();
        MessageBoxWarning(software_name, "Another profile is stopping...");
        mu_starting.unlock();
        return;
    }
    mu_stopping.unlock();
    start_pending = true;
    refresh_status();

    // check core state
    if (!ProxorGui::dataStore->core_running) {
        runOnUiThread(
            [=] {
                MW_show_log("Try to start the config, but the core has not listened to the grpc port, so restart it...");
                core_process->start_profile_when_core_is_up = ent->id;
                core_process->Restart();
            },
            DS_cores);
        mu_starting.unlock();
        return; // let CoreProcess call proxor_start when core is up
    }

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);

    runOnNewThread([=] {
        // validate config before stopping the current connection or marking anything active
#ifndef NKR_NO_GRPC
        libcore::LoadConfigReq validateReq;
        validateReq.set_core_config(QJsonObject2QString(result->coreConfig, false).toStdString());
        bool validateRpcOK;
        QString validateError = defaultClient->Validate(&validateRpcOK, validateReq);
        if (validateRpcOK && !validateError.isEmpty()) {
            MW_show_log("<<<<<<<< " + tr("Config validation failed for %1: %2").arg(ent->bean->DisplayTypeAndName(), validateError));
            runOnUiThread([=] { MessageBoxWarning("Validate return error", validateError); });
            start_pending = false;
            mu_starting.unlock();
            runOnUiThread([=] {
                refresh_status();
                restartMsgboxTimer->cancel();
                restartMsgboxTimer->deleteLater();
                restartMsgbox->deleteLater();
            });
            return;
        } else if (!validateRpcOK) {
            MW_show_log("<<<<<<<< " + tr("Config validation RPC failed for %1").arg(ent->bean->DisplayTypeAndName()));
            start_pending = false;
            mu_starting.unlock();
            runOnUiThread([=] {
                refresh_status();
                restartMsgboxTimer->cancel();
                restartMsgboxTimer->deleteLater();
                restartMsgbox->deleteLater();
            });
            return;
        }
#endif

        // stop current running
        if (ProxorGui::dataStore->started_id >= 0) {
            runOnUiThread([=] { proxor_stop(false, true); });
            sem_stopped.acquire();
        }
        // do start
        MW_show_log(">>>>>>>> " + tr("Starting profile %1").arg(ent->bean->DisplayTypeAndName()));
        if (!proxor_start_stage2()) {
            start_pending = false;
            MW_show_log("<<<<<<<< " + tr("Failed to start profile %1").arg(ent->bean->DisplayTypeAndName()));
        }
        mu_starting.unlock();
        // cancel timeout
        runOnUiThread([=] {
            refresh_status();
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
#ifdef Q_OS_LINUX
            // Check systemd-resolved
            if (ProxorGui::dataStore->spmode_vpn && ProxorGui::dataStore->routing->direct_dns.startsWith("local") && ReadFileText("/etc/resolv.conf").contains("systemd-resolved")) {
                MW_show_log("[Warning] The default Direct DNS may not works with systemd-resolved, you may consider change your DNS settings.");
            }
#endif
        });
    });
}

void MainWindow::proxor_stop(bool crash, bool sem) {
    auto id = ProxorGui::dataStore->started_id;
    if (id < 0) {
        if (sem) sem_stopped.release();
        return;
    }

    auto proxor_stop_stage2 = [=] {
        runOnUiThread(
            [=] {
                while (!ProxorGui_sys::running_ext.empty()) {
                    auto extC = ProxorGui_sys::running_ext.front();
                    extC->Kill();
                    ProxorGui_sys::running_ext.pop_front();
                }
            },
            DS_cores);

#ifndef NKR_NO_GRPC
        ProxorGui_traffic::trafficLooper->loop_enabled = false;
        ProxorGui_traffic::trafficLooper->loop_mutex.lock();
        if (ProxorGui::dataStore->traffic_loop_interval != 0) {
            ProxorGui_traffic::trafficLooper->UpdateAll();
            for (const auto &item: ProxorGui_traffic::trafficLooper->items) {
                ProxorGui::profileManager->SaveProfile(ProxorGui::profileManager->GetProfile(item->id));
                runOnUiThread([=] { refresh_proxy_list(item->id); });
            }
        }
        ProxorGui_traffic::trafficLooper->loop_mutex.unlock();

        if (!crash) {
            bool rpcOK;
            QString error = defaultClient->Stop(&rpcOK);
            if (rpcOK && !error.isEmpty()) {
                runOnUiThread([=] { MessageBoxWarning("Stop return error", error); });
                return false;
            } else if (!rpcOK) {
                return false;
            }
        }
#endif

        ProxorGui::dataStore->UpdateStartedId(-1919);
        ProxorGui::dataStore->need_keep_vpn_off = false;
        running = nullptr;

        runOnUiThread([=] {
            refresh_status();
            refresh_proxy_list(id);
            refresh_connection_list({});
        });

        return true;
    };

    if (!mu_stopping.tryLock()) {
        if (sem) sem_stopped.release();
        return;
    }

    // timeout message
    auto restartMsgbox = new QMessageBox(QMessageBox::Question, software_name, tr("If there is no response for a long time, it is recommended to restart the software."),
                                         QMessageBox::Yes | QMessageBox::No, this);
    connect(restartMsgbox, &QMessageBox::accepted, this, [=] { MW_dialog_message("", "RestartProgram"); });
    auto restartMsgboxTimer = new MessageBoxTimer(this, restartMsgbox, 5000);

    runOnNewThread([=] {
        // do stop
        MW_show_log(">>>>>>>> " + tr("Stopping profile %1").arg(running->bean->DisplayTypeAndName()));
        if (!proxor_stop_stage2()) {
            MW_show_log("<<<<<<<< " + tr("Failed to stop, please restart the program."));
        }
        mu_stopping.unlock();
        if (sem) sem_stopped.release();
        // cancel timeout
        runOnUiThread([=] {
            restartMsgboxTimer->cancel();
            restartMsgboxTimer->deleteLater();
            restartMsgbox->deleteLater();
        });
    });
}

void MainWindow::CheckUpdate(bool silent) {
    // on new thread...
#ifndef NKR_NO_GRPC
    bool ok;
    libcore::UpdateReq request;
    request.set_action(libcore::UpdateAction::Check);
    request.set_check_pre_release(ProxorGui::dataStore->check_include_pre);
    auto response = ProxorGui_rpc::defaultClient->Update(&ok, request);
    if (!ok) return;

    auto err = response.error();
    if (!err.empty()) {
        if (silent) return;
        runOnUiThread([=] {
            MessageBoxWarning(QObject::tr("Update"), err.c_str());
        });
        return;
    }

    if (response.download_url().empty()) {
        if (silent) return;
        runOnUiThread([=] {
            MessageBoxInfo(QObject::tr("Check for Updates"), QObject::tr("You are already up to date."));
        });
        return;
    }

    runOnUiThread([=] {
        auto allow_updater = !ProxorGui::dataStore->flag_use_appdata;
        auto notePreRelease = response.is_pre_release() ? QObject::tr("Prerelease") : QObject::tr("Release");
        auto releasePageUrl = QUrl(response.release_url().c_str());
        QString details = QObject::tr("Current version: %1\nAvailable package: %2\nChannel: %3")
                              .arg(QString(NKR_VERSION), response.assets_name().c_str(), notePreRelease);
        if (!response.release_note().empty()) {
            details += QObject::tr("\n\nRelease notes:\n%1").arg(response.release_note().c_str());
        }
        if (!allow_updater) {
            details += QObject::tr("\n\nAutomatic installation is disabled in appdata mode.");
        }

        bool dialogClosed = false;
        while (!dialogClosed) {
            QMessageBox box(QMessageBox::Question, QObject::tr("Update Available"), details, QMessageBox::NoButton, this);
            QAbstractButton *downloadButton = nullptr;
            if (allow_updater) {
                downloadButton = box.addButton(QObject::tr("Download and Restart"), QMessageBox::AcceptRole);
            }
            QAbstractButton *openButton = box.addButton(QObject::tr("Open Release Page"), QMessageBox::AcceptRole);
            box.addButton(QObject::tr("Cancel"), QMessageBox::RejectRole);
            box.exec();

            if (downloadButton == box.clickedButton() && allow_updater) {
                dialogClosed = true;
                updateProgressDialog = new UpdateProgressDialog(response.assets_name().c_str(), this);
                connect(updateProgressDialog, &UpdateProgressDialog::downloadComplete, this, &MainWindow::onUpdateStaged);
                updateProgressDialog->show();

                runOnNewThread([=] {
                    bool ok2;
                    libcore::UpdateReq request2;
                    request2.set_action(libcore::UpdateAction::Download);
                    auto response2 = ProxorGui_rpc::defaultClient->Update(&ok2, request2);
                    if (!ok2) return;

                    if (!response2.error().empty()) {
                        runOnUiThread([=] {
                            MessageBoxWarning(QObject::tr("Update"), response2.error().c_str());
                        });
                    }
                });
            } else if (openButton == box.clickedButton() && releasePageUrl.isValid()) {
                QDesktopServices::openUrl(releasePageUrl);
            } else {
                dialogClosed = true;
            }
        }
    });
#endif
}
