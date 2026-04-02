#include "Database.hpp"

#include "fmt/includes.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace {
    const QString kDatabaseFile = QStringLiteral("proxor.db");
    const QString kGroupsTabOrderKey = QStringLiteral("groups_tab_order");
    const QString kProfileManagerJson = QStringLiteral("groups/pm.json");

    bool RenameToBackup(const QString &path) {
        if (!QFile::exists(path)) return true;
        const auto backupPath = path + QStringLiteral(".bak");
        if (QFile::exists(backupPath) && !QFile::remove(backupPath)) {
            return false;
        }
        return QFile::rename(path, backupPath);
    }
} // namespace

namespace ProxorGui {

    namespace {
        std::shared_ptr<ProxyEntity> LoadProxyEntityFromBytes(const QByteArray &data) {
            QJsonParseError error{};
            const auto document = QJsonDocument::fromJson(data, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) {
                return nullptr;
            }

            const auto type = document.object().value(QStringLiteral("type")).toString();
            auto ent = ProfileManager::NewProxyEntity(type);
            if (ent == nullptr || ent->bean == nullptr || ent->bean->version == -114514) {
                return nullptr;
            }

            ent->FromJsonBytes(data);
            ent->RefreshSummaryFromBean();
            return ent;
        }

        std::shared_ptr<Group> LoadGroupFromBytes(const QByteArray &data) {
            auto ent = ProfileManager::NewGroup();
            ent->FromJsonBytes(data);
            return ent;
        }
    } // namespace

    ProfileManager *profileManager = new ProfileManager();

    ProfileManager::ProfileManager() : JsonStore() {
        save_control_no_save = true;
    }

    QList<int> filterIntJsonFile(const QString &path) {
        QList<int> result;
        QDir dr(path);
        auto entryList = dr.entryList(QDir::Files);
        for (auto e: entryList) {
            e = e.toLower();
            if (!e.endsWith(".json", Qt::CaseInsensitive)) continue;
            e = e.remove(".json", Qt::CaseInsensitive);
            bool ok;
            const auto id = e.toInt(&ok);
            if (ok) {
                result << id;
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    void ProfileManager::InitSchema() {
        if (m_db == nullptr) return;

        m_db->exec("PRAGMA journal_mode=WAL");
        m_db->exec(
            "CREATE TABLE IF NOT EXISTS groups ("
            "  id   INTEGER PRIMARY KEY,"
            "  data TEXT NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS profiles ("
            "  id   INTEGER PRIMARY KEY,"
            "  data TEXT NOT NULL"
            ");"
            "CREATE TABLE IF NOT EXISTS settings ("
            "  key   TEXT PRIMARY KEY,"
            "  value TEXT NOT NULL"
            ");");

        if (m_db->execAndGet("PRAGMA user_version").getInt() == 0) {
            m_db->exec("PRAGMA user_version = 1");
        }
    }

    bool ProfileManager::SaveProfileToDb(int id) {
        if (m_db == nullptr) return false;

        const auto it = profiles.find(id);
        if (it == profiles.end() || it->second == nullptr) return false;

        const auto blob = it->second->ToJsonBytes();
        const auto changed = it->second->last_save_content != blob;
        it->second->last_save_content = blob;

        SQLite::Statement stmt(*m_db, "INSERT OR REPLACE INTO profiles (id, data) VALUES (?, ?)");
        stmt.bind(1, id);
        stmt.bind(2, blob.toStdString());
        stmt.exec();

        return changed;
    }

    bool ProfileManager::SaveGroupToDb(int id) {
        if (m_db == nullptr) return false;

        const auto it = groups.find(id);
        if (it == groups.end() || it->second == nullptr) return false;

        const auto blob = it->second->ToJsonBytes();
        const auto changed = it->second->last_save_content != blob;
        it->second->last_save_content = blob;

        SQLite::Statement stmt(*m_db, "INSERT OR REPLACE INTO groups (id, data) VALUES (?, ?)");
        stmt.bind(1, id);
        stmt.bind(2, blob.toStdString());
        stmt.exec();

        return changed;
    }

    void ProfileManager::DeleteProfileFromDb(int id) {
        if (m_db == nullptr) return;

        SQLite::Statement stmt(*m_db, "DELETE FROM profiles WHERE id = ?");
        stmt.bind(1, id);
        stmt.exec();
    }

    void ProfileManager::DeleteGroupFromDb(int id) {
        if (m_db == nullptr) return;

        SQLite::Statement stmt(*m_db, "DELETE FROM groups WHERE id = ?");
        stmt.bind(1, id);
        stmt.exec();
    }

    void ProfileManager::SaveGroupsTabOrder() {
        if (m_db == nullptr) return;

        QJsonArray arr;
        for (auto id: groupsTabOrder) {
            arr.append(id);
        }

        SQLite::Statement stmt(*m_db, "INSERT OR REPLACE INTO settings (key, value) VALUES (?, ?)");
        stmt.bind(1, kGroupsTabOrderKey.toStdString());
        stmt.bind(2, QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)).toStdString());
        stmt.exec();
    }

    void ProfileManager::RebuildGroupProfileIndex() {
        groupProfileIds.clear();
        for (const auto &[gid, _]: groups) {
            groupProfileIds[gid] = {};
        }
        for (const auto &[id, profile]: profiles) {
            if (profile == nullptr) continue;
            AddProfileToGroupIndex(id, profile->gid);
        }
    }

    void ProfileManager::AddProfileToGroupIndex(int profileId, int gid) {
        if (gid < 0) return;
        auto &ids = groupProfileIds[gid];
        if (!ids.contains(profileId)) {
            ids.push_back(profileId);
            std::sort(ids.begin(), ids.end());
        }
    }

    void ProfileManager::RemoveProfileFromGroupIndex(int profileId, int gid) {
        if (!groupProfileIds.contains(gid)) return;
        auto &ids = groupProfileIds[gid];
        ids.removeAll(profileId);
    }

    void ProfileManager::WireEntityCallbacks(const std::shared_ptr<ProxyEntity> &ent) {
        if (ent == nullptr) return;

        std::weak_ptr<ProxyEntity> weakEnt = ent;
        ent->callback_before_save = [this, weakEnt]() {
            if (auto locked = weakEnt.lock()) {
                locked->RefreshSummaryFromBean();
                SaveProfileToDb(locked->id);
            }
        };
        ent->save_control_no_save = true;
    }

    void ProfileManager::WireGroupCallbacks(const std::shared_ptr<Group> &group) {
        if (group == nullptr) return;

        std::weak_ptr<Group> weakGroup = group;
        group->callback_before_save = [this, weakGroup]() {
            if (auto locked = weakGroup.lock()) {
                SaveGroupToDb(locked->id);
            }
        };
        group->save_control_no_save = true;
    }

    void ProfileManager::MigrateFromJson() {
        if (m_db == nullptr) return;

        const auto groupIds = filterIntJsonFile(QStringLiteral("groups"));
        const auto profileIds = filterIntJsonFile(QStringLiteral("profiles"));
        QList<QString> backupPaths;

        SQLite::Transaction tx(*m_db);

        for (auto id: groupIds) {
            const auto path = QStringLiteral("groups/%1.json").arg(id);
            backupPaths << path;

            auto ent = LoadGroup(path);
            if (ent == nullptr) continue;

            SQLite::Statement stmt(*m_db, "INSERT OR REPLACE INTO groups (id, data) VALUES (?, ?)");
            stmt.bind(1, ent->id >= 0 ? ent->id : id);
            stmt.bind(2, ent->ToJsonBytes().toStdString());
            stmt.exec();
        }

        for (auto id: profileIds) {
            const auto path = QStringLiteral("profiles/%1.json").arg(id);
            backupPaths << path;

            auto ent = LoadProxyEntity(path);
            if (ent == nullptr || ent->bean == nullptr || ent->bean->version == -114514) continue;

            SQLite::Statement stmt(*m_db, "INSERT OR REPLACE INTO profiles (id, data) VALUES (?, ?)");
            stmt.bind(1, ent->id >= 0 ? ent->id : id);
            stmt.bind(2, ent->ToJsonBytes().toStdString());
            stmt.exec();
        }

        if (QFile::exists(kProfileManagerJson)) {
            backupPaths << kProfileManagerJson;

            QList<int> loadedOrder;
            JsonStore pmStore(kProfileManagerJson);
            pmStore._add(new configItem("groups", &loadedOrder, itemType::integerList));
            pmStore.Load();
            groupsTabOrder = loadedOrder;
            SaveGroupsTabOrder();
        }

        tx.commit();

        for (const auto &path: backupPaths) {
            RenameToBackup(path);
        }
    }

    void ProfileManager::LoadFromSqlite() {
        if (m_db == nullptr) return;

        profiles = {};
        groups = {};
        profilesIdOrder = {};
        groupsIdOrder = {};
        groupsTabOrder = {};
        groupProfileIds = {};

        SQLite::Statement groupsQuery(*m_db, "SELECT id, data FROM groups ORDER BY id");
        while (groupsQuery.executeStep()) {
            const auto id = groupsQuery.getColumn(0).getInt();
            const auto blob = QByteArray::fromStdString(groupsQuery.getColumn(1).getString());

            auto ent = LoadGroupFromBytes(blob);
            if (ent == nullptr) continue;

            ent->id = id;
            ent->last_save_content = blob;
            WireGroupCallbacks(ent);
            groups[id] = ent;
            groupsIdOrder << id;
        }

        SQLite::Statement profilesQuery(*m_db, "SELECT id, data FROM profiles ORDER BY id");
        while (profilesQuery.executeStep()) {
            const auto id = profilesQuery.getColumn(0).getInt();
            const auto blob = QByteArray::fromStdString(profilesQuery.getColumn(1).getString());

            auto ent = LoadProxyEntityFromBytes(blob);
            if (ent == nullptr || ent->bean == nullptr || ent->bean->version == -114514) {
                continue;
            }

            ent->id = id;
            ent->last_save_content = blob;
            WireEntityCallbacks(ent);
            profiles[id] = ent;
            profilesIdOrder << id;
        }

        SQLite::Statement orderQuery(*m_db, "SELECT value FROM settings WHERE key = ?");
        orderQuery.bind(1, kGroupsTabOrderKey.toStdString());
        if (orderQuery.executeStep()) {
            QJsonParseError error{};
            const auto orderDoc = QJsonDocument::fromJson(QByteArray::fromStdString(orderQuery.getColumn(0).getString()), &error);
            if (error.error == QJsonParseError::NoError && orderDoc.isArray()) {
                groupsTabOrder = QJsonArray2QListInt(orderDoc.array());
            }
        }

        QList<int> normalizedOrder;
        for (auto id: groupsTabOrder) {
            if (groups.count(id)) {
                normalizedOrder << id;
            }
        }
        for (auto id: groupsIdOrder) {
            if (!normalizedOrder.contains(id)) {
                normalizedOrder << id;
            }
        }
        groupsTabOrder = normalizedOrder;
        RebuildGroupProfileIndex();
    }

    void ProfileManager::LoadManager() {
        profiles = {};
        groups = {};
        profilesIdOrder = {};
        groupsIdOrder = {};
        groupsTabOrder = {};
        groupProfileIds = {};

        const auto dbExists = QFile::exists(kDatabaseFile);
        const auto jsonExists = !filterIntJsonFile(QStringLiteral("profiles")).isEmpty()
                             || !filterIntJsonFile(QStringLiteral("groups")).isEmpty()
                             || QFile::exists(kProfileManagerJson);

        if (dbExists) {
            m_db = std::make_unique<SQLite::Database>(kDatabaseFile.toStdString(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            InitSchema();
            LoadFromSqlite();
        } else {
            m_db = std::make_unique<SQLite::Database>(kDatabaseFile.toStdString(), SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);
            InitSchema();
            if (jsonExists) {
                MigrateFromJson();
            }
            LoadFromSqlite();
        }

        if (groups.empty()) {
            auto defaultGroup = ProxorGui::ProfileManager::NewGroup();
            defaultGroup->name = QObject::tr("Default");
            ProxorGui::profileManager->AddGroup(defaultGroup);
        }

        if (!groups.count(dataStore->current_group) && !groupsTabOrder.isEmpty()) {
            dataStore->current_group = groupsTabOrder.first();
        }

        if (dataStore->flag_reorder) {
            std::map<int, int> gidOld2New;
            std::map<int, int> pidOld2New;
            std::map<int, std::shared_ptr<ProxyEntity>> newProfiles;
            std::map<int, std::shared_ptr<Group>> newGroups;
            QList<int> newProfilesIdOrder;
            QList<int> newGroupsIdOrder;

            int nextProfileId = 0;
            int nextGroupId = 0;

            for (auto oldGid: groupsTabOrder) {
                auto group = GetGroup(oldGid);
                if (group == nullptr) continue;

                const auto newGid = nextGroupId++;
                gidOld2New[oldGid] = newGid;

                for (const auto &profile: group->ProfilesWithOrder()) {
                    if (profile == nullptr) continue;

                    pidOld2New[profile->id] = nextProfileId;
                    profile->id = nextProfileId;
                    profile->gid = newGid;
                    newProfiles[nextProfileId] = profile;
                    newProfilesIdOrder << nextProfileId;
                    nextProfileId++;
                }
            }

            for (auto oldGid: groupsTabOrder) {
                auto group = GetGroup(oldGid);
                if (group == nullptr || !gidOld2New.count(oldGid)) continue;

                QList<int> newToggleProxyIds;
                for (auto oldId: group->toggle_proxy_ids) {
                    if (pidOld2New.count(oldId)) {
                        newToggleProxyIds << pidOld2New[oldId];
                    }
                }

                group->toggle_proxy_ids = newToggleProxyIds;
                group->order = {};
                group->id = gidOld2New[oldGid];
                newGroups[group->id] = group;
                newGroupsIdOrder << group->id;
            }

            if (gidOld2New.count(dataStore->current_group)) {
                dataStore->current_group = gidOld2New[dataStore->current_group];
            }
            if (pidOld2New.count(dataStore->started_id)) {
                dataStore->started_id = pidOld2New[dataStore->started_id];
            }
            if (pidOld2New.count(dataStore->remember_id)) {
                dataStore->remember_id = pidOld2New[dataStore->remember_id];
            }

            SQLite::Transaction tx(*m_db);
            m_db->exec("DELETE FROM profiles");
            m_db->exec("DELETE FROM groups");

            profiles = newProfiles;
            groups = newGroups;
            profilesIdOrder = newProfilesIdOrder;
            groupsIdOrder = newGroupsIdOrder;
            groupsTabOrder = newGroupsIdOrder;
            RebuildGroupProfileIndex();

            for (const auto &[id, group]: groups) {
                WireGroupCallbacks(group);
                SaveGroupToDb(id);
            }
            for (const auto &[id, profile]: profiles) {
                WireEntityCallbacks(profile);
                SaveProfileToDb(id);
            }
            SaveGroupsTabOrder();
            tx.commit();

            MessageBoxInfo(software_name, "Profiles and groups reorder complete.");
        }
    }

    void ProfileManager::SaveManager() {
        SaveGroupsTabOrder();
    }

    std::shared_ptr<ProxyEntity> ProfileManager::LoadProxyEntity(const QString &jsonPath) {
        ProxyEntity ent0(nullptr, nullptr);
        ent0.fn = jsonPath;
        const auto validJson = ent0.Load();
        const auto type = ent0.type;

        std::shared_ptr<ProxyEntity> ent;
        auto validType = validJson;

        if (validType) {
            ent = NewProxyEntity(type);
            validType = ent->bean->version != -114514;
        }

        if (validType) {
            ent->load_control_must = true;
            ent->fn = jsonPath;
            ent->Load();
        }
        return ent;
    }

    std::shared_ptr<ProxyEntity> ProfileManager::NewProxyEntity(const QString &type) {
        ProxorGui_fmt::AbstractBean *bean;

        if (type == "socks") {
            bean = new ProxorGui_fmt::SocksHttpBean(ProxorGui_fmt::SocksHttpBean::type_Socks5);
        } else if (type == "http") {
            bean = new ProxorGui_fmt::SocksHttpBean(ProxorGui_fmt::SocksHttpBean::type_HTTP);
        } else if (type == "shadowsocks") {
            bean = new ProxorGui_fmt::ShadowSocksBean();
        } else if (type == "chain") {
            bean = new ProxorGui_fmt::ChainBean();
        } else if (type == "vmess") {
            bean = new ProxorGui_fmt::VMessBean();
        } else if (type == "trojan") {
            bean = new ProxorGui_fmt::TrojanVLESSBean(ProxorGui_fmt::TrojanVLESSBean::proxy_Trojan);
        } else if (type == "vless") {
            bean = new ProxorGui_fmt::TrojanVLESSBean(ProxorGui_fmt::TrojanVLESSBean::proxy_VLESS);
        } else if (type == "naive") {
            bean = new ProxorGui_fmt::NaiveBean();
        } else if (type == "hysteria2") {
            bean = new ProxorGui_fmt::QUICBean(ProxorGui_fmt::QUICBean::proxy_Hysteria2);
        } else if (type == "tuic") {
            bean = new ProxorGui_fmt::QUICBean(ProxorGui_fmt::QUICBean::proxy_TUIC);
        } else if (type == "custom") {
            bean = new ProxorGui_fmt::CustomBean();
        } else {
            bean = new ProxorGui_fmt::AbstractBean(-114514);
        }

        auto ent = std::make_shared<ProxyEntity>(bean, type);
        return ent;
    }

    std::shared_ptr<Group> ProfileManager::NewGroup() {
        auto ent = std::make_shared<Group>();
        return ent;
    }

    ProxyEntity::ProxyEntity(ProxorGui_fmt::AbstractBean *bean, const QString &type_) {
        if (type_ != nullptr) this->type = type_;

        _add(new configItem("type", &type, itemType::string));
        _add(new configItem("id", &id, itemType::integer));
        _add(new configItem("gid", &gid, itemType::integer));
        _add(new configItem("yc", &latency, itemType::integer));
        _add(new configItem("report", &full_test_report, itemType::string));
        _add(new configItem("summary_name", &summary_name, itemType::string));
        _add(new configItem("summary_addr", &summary_serverAddress, itemType::string));
        _add(new configItem("summary_port", &summary_serverPort, itemType::integer));
        _add(new configItem("summary_display_type", &summary_displayType, itemType::string));
        _add(new configItem("summary_display_addr", &summary_displayAddress, itemType::string));
        _add(new configItem("summary_display_name", &summary_displayName, itemType::string));
        _add(new configItem("summary_display_type_name", &summary_displayTypeAndName, itemType::string));

        if (bean != nullptr) {
            this->bean = std::shared_ptr<ProxorGui_fmt::AbstractBean>(bean);
            _add(new configItem("bean", dynamic_cast<JsonStore *>(bean), itemType::jsonStore));
            _add(new configItem("traffic", dynamic_cast<JsonStore *>(traffic_data.get()), itemType::jsonStore));
            RefreshSummaryFromBean();
        }
    };

    void ProxyEntity::RefreshSummaryFromBean() {
        if (bean == nullptr) return;
        summary_name = bean->name;
        summary_serverAddress = bean->serverAddress;
        summary_serverPort = bean->serverPort;
        summary_displayType = bean->DisplayType();
        summary_displayAddress = bean->DisplayAddress();
        summary_displayName = bean->DisplayName();
        summary_displayTypeAndName = bean->DisplayTypeAndName();
    }

    bool ProxyEntity::EnsureHydrated() const {
        return bean != nullptr;
    }

    QString ProxyEntity::DisplayTypeSummary() const {
        return summary_displayType;
    }

    QString ProxyEntity::DisplayAddressSummary() const {
        return summary_displayAddress;
    }

    QString ProxyEntity::DisplayNameSummary() const {
        return summary_displayName;
    }

    QString ProxyEntity::DisplayTypeAndNameSummary() const {
        return summary_displayTypeAndName;
    }

    QString ProxyEntity::DisplayLatency() const {
        if (latency < 0) {
            return QObject::tr("Unavailable");
        } else if (latency > 0) {
            return UNICODE_LRO + QStringLiteral("%1 ms").arg(latency);
        } else {
            return "";
        }
    }

    QColor ProxyEntity::DisplayLatencyColor() const {
        if (latency < 0) {
            return Qt::red;
        } else if (latency > 0) {
            auto greenMs = dataStore->test_latency_url.startsWith("https://") ? 200 : 100;
            if (latency < greenMs) {
                return Qt::darkGreen;
            } else {
                return Qt::darkYellow;
            }
        } else {
            return {};
        }
    }

    int ProfileManager::NewProfileID() const {
        if (profiles.empty()) {
            return 0;
        } else {
            return profilesIdOrder.last() + 1;
        }
    }

    bool ProfileManager::AddProfile(const std::shared_ptr<ProxyEntity> &ent, int gid) {
        if (ent == nullptr || ent->id >= 0) {
            return false;
        }

        ent->gid = gid < 0 ? dataStore->current_group : gid;
        if (!groups.count(ent->gid) && !groupsTabOrder.isEmpty()) {
            ent->gid = groupsTabOrder.first();
        }

        ent->id = NewProfileID();
        ent->RefreshSummaryFromBean();
        profiles[ent->id] = ent;
        profilesIdOrder.push_back(ent->id);
        AddProfileToGroupIndex(ent->id, ent->gid);
        WireEntityCallbacks(ent);

        auto group = GetGroup(ent->gid);
        if (group != nullptr) {
            if (group->toggle_proxy_ids.isEmpty()) {
                group->toggle_proxy_ids.push_back(ent->id);
            }
            SaveGroup(group);
        }

        return SaveProfile(ent);
    }

    bool ProfileManager::SaveProfile(const std::shared_ptr<ProxyEntity> &ent) {
        if (ent == nullptr || ent->id < 0) return false;
        if (!profiles.count(ent->id)) {
            ent->RefreshSummaryFromBean();
            profiles[ent->id] = ent;
            if (!profilesIdOrder.contains(ent->id)) {
                profilesIdOrder << ent->id;
                std::sort(profilesIdOrder.begin(), profilesIdOrder.end());
            }
            AddProfileToGroupIndex(ent->id, ent->gid);
            WireEntityCallbacks(ent);
        }
        ent->RefreshSummaryFromBean();
        return SaveProfileToDb(ent->id);
    }

    void ProfileManager::DeleteProfile(int id) {
        if (id < 0 || dataStore->started_id == id) return;

        auto ent = GetProfile(id);
        if (ent != nullptr) {
            auto group = GetGroup(ent->gid);
            if (group != nullptr) {
                group->toggle_proxy_ids.removeAll(id);
                group->order.removeAll(id);
                SaveGroup(group);
            }
            RemoveProfileFromGroupIndex(id, ent->gid);
        }

        profiles.erase(id);
        profilesIdOrder.removeAll(id);
        DeleteProfileFromDb(id);
    }

    void ProfileManager::MoveProfile(const std::shared_ptr<ProxyEntity> &ent, int gid) {
        if (ent == nullptr || gid == ent->gid || gid < 0) return;

        auto oldGroup = GetGroup(ent->gid);
        if (oldGroup != nullptr) {
            oldGroup->order.removeAll(ent->id);
            oldGroup->toggle_proxy_ids.removeAll(ent->id);
            SaveGroup(oldGroup);
        }
        RemoveProfileFromGroupIndex(ent->id, ent->gid);

        auto newGroup = GetGroup(gid);
        if (newGroup != nullptr) {
            if (!newGroup->order.isEmpty()) {
                newGroup->order.push_back(ent->id);
            }
            if (newGroup->toggle_proxy_ids.isEmpty()) {
                newGroup->toggle_proxy_ids.push_back(ent->id);
            }
            SaveGroup(newGroup);
        }

        ent->gid = gid;
        AddProfileToGroupIndex(ent->id, gid);
        SaveProfile(ent);
    }

    std::shared_ptr<ProxyEntity> ProfileManager::GetProfile(int id) {
        return profiles.count(id) ? profiles[id] : nullptr;
    }

    QList<int> ProfileManager::GroupProfileIds(int gid) const {
        return groupProfileIds.value(gid);
    }

    QList<std::shared_ptr<ProxyEntity>> ProfileManager::GroupProfiles(int gid) const {
        QList<std::shared_ptr<ProxyEntity>> ret;
        const auto ids = GroupProfileIds(gid);
        ret.reserve(ids.size());
        for (auto id: ids) {
            const auto it = profiles.find(id);
            if (it != profiles.end() && it->second != nullptr) {
                ret += it->second;
            }
        }
        return ret;
    }

    Group::Group() {
        _add(new configItem("id", &id, itemType::integer));
        _add(new configItem("front_proxy_id", &front_proxy_id, itemType::integer));
        _add(new configItem("archive", &archive, itemType::boolean));
        _add(new configItem("skip_auto_update", &skip_auto_update, itemType::boolean));
        _add(new configItem("name", &name, itemType::string));
        _add(new configItem("order", &order, itemType::integerList));
        _add(new configItem("url", &url, itemType::string));
        _add(new configItem("info", &info, itemType::string));
        _add(new configItem("lastup", &sub_last_update, itemType::integer64));
        _add(new configItem("manually_column_width", &manually_column_width, itemType::boolean));
        _add(new configItem("column_width", &column_width, itemType::integerList));
        _add(new configItem("toggle_proxy_ids", &toggle_proxy_ids, itemType::integerList));
    }

    std::shared_ptr<Group> ProfileManager::LoadGroup(const QString &jsonPath) {
        auto ent = std::make_shared<Group>();
        ent->fn = jsonPath;
        ent->Load();
        return ent;
    }

    int ProfileManager::NewGroupID() const {
        if (groups.empty()) {
            return 0;
        } else {
            return groupsIdOrder.last() + 1;
        }
    }

    bool ProfileManager::AddGroup(const std::shared_ptr<Group> &ent) {
        if (ent == nullptr || ent->id >= 0) {
            return false;
        }

        ent->id = NewGroupID();
        groups[ent->id] = ent;
        groupsIdOrder.push_back(ent->id);
        groupsTabOrder.push_back(ent->id);
        groupProfileIds[ent->id] = {};
        WireGroupCallbacks(ent);

        const auto saved = SaveGroup(ent);
        SaveGroupsTabOrder();

        if (dataStore->current_group < 0 || !groups.count(dataStore->current_group)) {
            dataStore->current_group = ent->id;
        }

        return saved;
    }

    bool ProfileManager::SaveGroup(const std::shared_ptr<Group> &ent) {
        if (ent == nullptr || ent->id < 0) return false;
        if (!groups.count(ent->id)) {
            groups[ent->id] = ent;
            if (!groupsIdOrder.contains(ent->id)) {
                groupsIdOrder << ent->id;
                std::sort(groupsIdOrder.begin(), groupsIdOrder.end());
            }
            if (!groupsTabOrder.contains(ent->id)) {
                groupsTabOrder << ent->id;
            }
            if (!groupProfileIds.contains(ent->id)) {
                groupProfileIds[ent->id] = {};
            }
            WireGroupCallbacks(ent);
        }
        return SaveGroupToDb(ent->id);
    }

    void ProfileManager::DeleteGroup(int gid) {
        if (groups.size() <= 1) return;

        auto toDelete = GroupProfileIds(gid);
        for (const auto &id: toDelete) {
            DeleteProfile(id);
        }

        groups.erase(gid);
        groupsIdOrder.removeAll(gid);
        groupsTabOrder.removeAll(gid);
        groupProfileIds.remove(gid);
        DeleteGroupFromDb(gid);
        SaveGroupsTabOrder();

        if (dataStore->current_group == gid) {
            dataStore->current_group = groupsTabOrder.isEmpty() ? -1 : groupsTabOrder.first();
        }
    }

    std::shared_ptr<Group> ProfileManager::GetGroup(int id) {
        return groups.count(id) ? groups[id] : nullptr;
    }

    std::shared_ptr<Group> ProfileManager::CurrentGroup() {
        return GetGroup(dataStore->current_group);
    }

    QList<std::shared_ptr<ProxyEntity>> Group::Profiles() const {
        return profileManager->GroupProfiles(id);
    }

    QList<std::shared_ptr<ProxyEntity>> Group::ProfilesWithOrder() const {
        if (order.isEmpty()) {
            return Profiles();
        } else {
            QList<std::shared_ptr<ProxyEntity>> ret;
            for (auto _id: order) {
                auto ent = profileManager->GetProfile(_id);
                if (ent != nullptr) ret += ent;
            }
            return ret;
        }
    }

} // namespace ProxorGui
