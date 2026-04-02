#include "GroupListModel.h"

#include <QColor>
#include <QRegularExpression>

#include "db/Database.hpp"
#include "db/Group.hpp"
#include "main/GuiUtils.hpp"

namespace {
QString parseSubInfo(const QString &info) {
    if (info.trimmed().isEmpty()) return {};

    long long used = 0;
    long long total = 0;
    long long expire = 0;

    auto totalMatch = QRegularExpression("total=([0-9]+)").match(info);
    if (totalMatch.lastCapturedIndex() >= 1) {
        total = totalMatch.captured(1).toLongLong();
    } else {
        return {};
    }
    auto uploadMatch = QRegularExpression("upload=([0-9]+)").match(info);
    if (uploadMatch.lastCapturedIndex() >= 1) {
        used += uploadMatch.captured(1).toLongLong();
    }
    auto downloadMatch = QRegularExpression("download=([0-9]+)").match(info);
    if (downloadMatch.lastCapturedIndex() >= 1) {
        used += downloadMatch.captured(1).toLongLong();
    }
    auto expireMatch = QRegularExpression("expire=([0-9]+)").match(info);
    if (expireMatch.lastCapturedIndex() >= 1) {
        expire = expireMatch.captured(1).toLongLong();
    }

    return QObject::tr("Used: %1 Remain: %2 Expire: %3")
        .arg(ReadableSize(used), ReadableSize(total - used), DisplayTime(expire, QLocale::ShortFormat));
}

QString groupTypeText(const std::shared_ptr<ProxorGui::Group> &group) {
    if (group == nullptr) return {};
    auto type = group->url.isEmpty() ? QObject::tr("Basic") : QObject::tr("Subscription");
    if (group->archive) type = QObject::tr("Archive") + " " + type;
    type += " (" + Int2String(group->Profiles().length()) + ")";
    return type;
}

QString groupInfoText(const std::shared_ptr<ProxorGui::Group> &group) {
    if (group == nullptr || group->url.isEmpty()) return {};

    QStringList info;
    if (group->sub_last_update != 0) {
        info << QObject::tr("Last update: %1").arg(DisplayTime(group->sub_last_update, QLocale::ShortFormat));
    }
    if (!group->info.isEmpty()) {
        const auto subInfo = parseSubInfo(group->info);
        if (!subInfo.isEmpty()) info << subInfo;
    }
    return info.join(" | ");
}
}

GroupListModel::GroupListModel(QObject *parent) : QAbstractTableModel(parent) {
}

int GroupListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_groupIds.size();
}

int GroupListModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant GroupListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    const auto group = ProxorGui::profileManager->GetGroup(groupIdAtRow(index.row()));
    if (group == nullptr) return {};

    if (role == GroupIdRole) return group->id;

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case TypeColumn:
                return groupTypeText(group);
            case NameColumn:
                return group->name;
            case UrlColumn:
                return group->url;
            case InfoColumn:
                return groupInfoText(group);
            case UpdateColumn:
                return group->url.isEmpty() ? QString() : QObject::tr("Update");
            case EditColumn:
                return QObject::tr("Edit");
            case RemoveColumn:
                return QObject::tr("Remove");
            default:
                return {};
        }
    }

    if (role == Qt::TextAlignmentRole && index.column() >= UpdateColumn) {
        return Qt::AlignCenter;
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == TypeColumn) return QColor(251, 114, 153);
        if (index.column() == UrlColumn) return QColor(102, 102, 102);
    }

    return {};
}

QVariant GroupListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};
    if (orientation != Qt::Horizontal) return {};

    switch (section) {
        case TypeColumn:
            return QObject::tr("Type");
        case NameColumn:
            return QObject::tr("Name");
        case UrlColumn:
            return QObject::tr("URL");
        case InfoColumn:
            return QObject::tr("Info");
        case UpdateColumn:
            return {};
        case EditColumn:
            return {};
        case RemoveColumn:
            return {};
        default:
            return {};
    }
}

Qt::ItemFlags GroupListModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void GroupListModel::reload() {
    beginResetModel();
    m_groupIds.clear();
    for (const auto id: ProxorGui::profileManager->groupsTabOrder) {
        if (ProxorGui::profileManager->GetGroup(id) != nullptr) {
            m_groupIds << id;
        }
    }
    endResetModel();
}

int GroupListModel::groupIdAtRow(int row) const {
    if (row < 0 || row >= m_groupIds.size()) return -1;
    return m_groupIds[row];
}
