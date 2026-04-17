#pragma once

#include "main/ProxorGui.hpp"
#include "ProxyEntity.hpp"

namespace ProxorGui {
    class Group : public JsonStore {
    public:
        int id = -1;
        bool archive = false;
        bool skip_auto_update = false;
        int sub_update_interval = 0;
        bool sub_update_always = false;
        QString name = "";
        QString url = "";
        QString info = "";
        qint64 sub_last_update = 0;
        int front_proxy_id = -1;

        // list ui
        bool manually_column_width = false;
        QList<int> column_width;
        QList<int> order;
        QList<int> toggle_proxy_ids;

        Group();

        // 按 id 顺序
        [[nodiscard]] QList<std::shared_ptr<ProxyEntity>> Profiles() const;

        // 按 显示 顺序
        [[nodiscard]] QList<std::shared_ptr<ProxyEntity>> ProfilesWithOrder() const;
    };
} // namespace ProxorGui
