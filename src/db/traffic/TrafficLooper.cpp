#include "TrafficLooper.hpp"

#include "rpc/gRPC.h"
#include "ui/mainwindow_interface.h"

#include <QThread>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QElapsedTimer>

namespace ProxorGui_traffic {

    TrafficLooper *trafficLooper = new TrafficLooper;
    QElapsedTimer elapsedTimer;

    TrafficData *TrafficLooper::update_stats(TrafficData *item) {
#ifndef NKR_NO_GRPC
        // last update
        auto now = elapsedTimer.elapsed();
        auto interval = now - item->last_update;
        item->last_update = now;
        if (interval <= 0) return nullptr;

        // query
        auto uplink = ProxorGui_rpc::defaultClient->QueryStats(item->tag, "uplink");
        auto downlink = ProxorGui_rpc::defaultClient->QueryStats(item->tag, "downlink");

        // add diff
        item->downlink += downlink;
        item->uplink += uplink;
        item->downlink_rate = downlink * 1000 / interval;
        item->uplink_rate = uplink * 1000 / interval;

        // return diff
        auto ret = new TrafficData(item->tag);
        ret->downlink = downlink;
        ret->uplink = uplink;
        ret->downlink_rate = item->downlink_rate;
        ret->uplink_rate = item->uplink_rate;
        return ret;
#endif
        return nullptr;
    }

    QJsonArray TrafficLooper::get_connection_list() {
#ifndef NKR_NO_GRPC
        auto str = ProxorGui_rpc::defaultClient->ListConnections();
        QJsonDocument jsonDocument = QJsonDocument::fromJson(str.c_str());
        return jsonDocument.array();
#else
        return QJsonArray{};
#endif
    }

    void TrafficLooper::UpdateAll() {
        std::map<std::string, TrafficData *> updated; // tag to diff
        for (const auto &item: this->items) {
            auto data = item.get();
            auto diff = updated[data->tag];
            // 避免重复查询一个 outbound tag
            if (diff == nullptr) {
                diff = update_stats(data);
                updated[data->tag] = diff;
            } else {
                data->uplink += diff->uplink;
                data->downlink += diff->downlink;
                data->uplink_rate = diff->uplink_rate;
                data->downlink_rate = diff->downlink_rate;
            }
        }
        updated[bypass->tag] = update_stats(bypass);
        //
        for (const auto &pair: updated) {
            delete pair.second;
        }
    }

    void TrafficLooper::Loop() {
        elapsedTimer.start();
        while (true) {
            auto sleep_ms = ProxorGui::dataStore->traffic_loop_interval;
            if (sleep_ms < 500 || sleep_ms > 5000) sleep_ms = 1000;
            QThread::msleep(sleep_ms);

            // profile start and stop
            if (!loop_enabled) {
                // 停止
                if (looping) {
                    looping = false;
                    runOnUiThread([=] {
                        auto m = GetMainWindow();
                        m->refresh_status("STOP");
                    });
                }
                continue;
            } else {
                // 开始
                if (!looping) {
                    looping = true;
                }
            }

            // do update
            loop_mutex.lock();

            if (ProxorGui::dataStore->traffic_loop_interval != 0) {
                UpdateAll();
            }

            QJsonArray conn_list;
            const auto *mainWindow = GetMainWindow();
            const bool shouldPollConnections = ProxorGui::dataStore->connection_statistics &&
                                               mainWindow != nullptr &&
                                               mainWindow->should_refresh_connection_statistics();
            if (shouldPollConnections) {
                conn_list = get_connection_list();
            }

            loop_mutex.unlock();

            // post to UI
            runOnUiThread([=] {
                auto m = GetMainWindow();
                if (m == nullptr) return;
                if (proxy != nullptr && ProxorGui::dataStore->traffic_loop_interval != 0) {
                    m->refresh_status(QObject::tr("Proxy: %1\nDirect: %2").arg(proxy->DisplaySpeed(), bypass->DisplaySpeed()));
                }
                if (ProxorGui::dataStore->traffic_loop_interval != 0) {
                    QList<int> updatedIds;
                    updatedIds.reserve(items.size());
                    for (const auto &item: items) {
                        if (item->id < 0) continue;
                        updatedIds += item->id;
                    }
                    m->refresh_proxy_list_rows(updatedIds);
                }
                if (!ProxorGui::dataStore->connection_statistics) {
                    m->refresh_connection_list({});
                } else if (shouldPollConnections) {
                    m->refresh_connection_list(conn_list);
                }
            });
        }
    }

} // namespace ProxorGui_traffic
