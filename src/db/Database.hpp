#pragma once

#include <memory>

#include <SQLiteCpp/SQLiteCpp.h>

#include "main/ProxorGui.hpp"
#include "ProxyEntity.hpp"
#include "Group.hpp"

namespace ProxorGui {
    class ProfileManager : private JsonStore {
    public:
        // JsonStore

        // order -> id
        QList<int> groupsTabOrder;

        // Manager

        std::map<int, std::shared_ptr<ProxyEntity>> profiles;
        std::map<int, std::shared_ptr<Group>> groups;

        ProfileManager();

        // LoadManager Reset and loads profiles & groups
        void LoadManager();

        void SaveManager();

        [[nodiscard]] static std::shared_ptr<ProxyEntity> NewProxyEntity(const QString &type);

        [[nodiscard]] static std::shared_ptr<Group> NewGroup();

        bool AddProfile(const std::shared_ptr<ProxyEntity> &ent, int gid = -1);

        bool SaveProfile(const std::shared_ptr<ProxyEntity> &ent);

        void DeleteProfile(int id);

        void MoveProfile(const std::shared_ptr<ProxyEntity> &ent, int gid);

        std::shared_ptr<ProxyEntity> GetProfile(int id);

        bool AddGroup(const std::shared_ptr<Group> &ent);

        bool SaveGroup(const std::shared_ptr<Group> &ent);

        void DeleteGroup(int gid);

        std::shared_ptr<Group> GetGroup(int id);

        std::shared_ptr<Group> CurrentGroup();

    private:
        // sort by id
        QList<int> profilesIdOrder;
        QList<int> groupsIdOrder;
        std::unique_ptr<SQLite::Database> m_db;

        [[nodiscard]] int NewProfileID() const;

        [[nodiscard]] int NewGroupID() const;

        void InitSchema();

        void MigrateFromJson();

        void LoadFromSqlite();

        bool SaveProfileToDb(int id);

        bool SaveGroupToDb(int id);

        void DeleteProfileFromDb(int id);

        void DeleteGroupFromDb(int id);

        void SaveGroupsTabOrder();

        void WireEntityCallbacks(const std::shared_ptr<ProxyEntity> &ent);

        void WireGroupCallbacks(const std::shared_ptr<Group> &group);

        static std::shared_ptr<ProxyEntity> LoadProxyEntity(const QString &jsonPath);

        static std::shared_ptr<Group> LoadGroup(const QString &jsonPath);
    };

    extern ProfileManager *profileManager;
} // namespace ProxorGui
