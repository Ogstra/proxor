#include "ProxyListModel.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>

#include <algorithm>

#include "db/Database.hpp"
#include "db/Group.hpp"
#include "db/ProxyEntity.hpp"
#include "main/GuiUtils.hpp"
#include "ui/Icon.hpp"

QString ProxyListModel::quotaText(const std::shared_ptr<ProxorGui::Group> &group) {
    if (group == nullptr || group->url.isEmpty()) return {};

    qint64 upload = 0;
    qint64 download = 0;
    qint64 total = 0;
    for (const auto &part: group->info.split(';')) {
        auto kv = part.trimmed().split('=');
        if (kv.size() != 2) continue;
        const auto key = kv[0].trimmed();
        const auto value = kv[1].trimmed().toLongLong();
        if (key == "upload") upload = value;
        else if (key == "download") download = value;
        else if (key == "total") total = value;
    }

    if (total <= 0) return {};

    const QString usedGB = QString::number(static_cast<double>(upload + download) / 1073741824.0, 'f', 2) + " GB";
    const QString totalGB = QString::number(static_cast<double>(total) / 1073741824.0, 'f', 2) + " GB";
    return usedGB + "/" + totalGB;
}

ProxyListModel::ProxyListModel(QObject *parent) : QAbstractTableModel(parent) {
}

int ProxyListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return m_rowIds.size();
}

int ProxyListModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant ProxyListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid()) return {};
    const auto profile = ProxorGui::profileManager->GetProfile(profileIdAtRow(index.row()));
    if (profile == nullptr) return {};

    const bool isRunning = profile->id == ProxorGui::dataStore->started_id;
    const bool isSelected = m_selectedIds.contains(profile->id);

    if (role == ProfileIdRole) return profile->id;

    if (index.column() == ToggleColumn) {
        if (role == Qt::CheckStateRole) {
            auto group = ProxorGui::profileManager->CurrentGroup();
            if (group == nullptr) return Qt::Unchecked;
            return group->toggle_proxy_ids.contains(profile->id) ? Qt::Checked : Qt::Unchecked;
        }
        if (role == Qt::DisplayRole) return {};
        if (role == Qt::TextAlignmentRole) return Qt::AlignCenter;
    }

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
            case NameColumn:
                return StripLeadingFlag(profile->summary_name);
            case TypeColumn:
                return profile->DisplayTypeSummary();
            case AddressColumn:
                return profile->DisplayAddressSummary();
            case TestResultColumn:
                return profile->DisplayLatency();
            case TrafficColumn:
                return profile->traffic_data->DisplayTraffic();
            default:
                return {};
        }
    }

    if (role == Qt::DecorationRole && index.column() == NameColumn) {
        const auto countryCode = LeadingFlagCountryCode(profile->summary_name);
        if (!countryCode.isEmpty()) return QIcon(Icon::GetCountryFlag(countryCode));
    }

    if (role == Qt::ForegroundRole) {
        if (index.column() == TestResultColumn) {
            const auto color = profile->DisplayLatencyColor();
            if (color.isValid()) return color;
        }
        if (isRunning && index.column() >= NameColumn && index.column() <= AddressColumn) {
            return QApplication::palette().link();
        }
    }

    return {};
}

bool ProxyListModel::setData(const QModelIndex &index, const QVariant &value, int role) {
    if (!index.isValid() || index.column() != ToggleColumn || role != Qt::CheckStateRole) return false;

    auto group = ProxorGui::profileManager->CurrentGroup();
    if (group == nullptr) return false;

    const int profileId = profileIdAtRow(index.row());
    const bool checked = static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked;

    if (checked) {
        group->toggle_proxy_ids = {profileId};
    } else {
        group->toggle_proxy_ids.removeAll(profileId);
    }
    ProxorGui::profileManager->SaveGroup(group);

    if (rowCount() > 0) {
        emit dataChanged(this->index(0, ToggleColumn), this->index(rowCount() - 1, ToggleColumn), {Qt::CheckStateRole});
    }
    return true;
}

QVariant ProxyListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) return {};

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case ToggleColumn:
                return {};
            case NameColumn:
                return QObject::tr("Name");
            case TypeColumn:
                return QObject::tr("Type");
            case AddressColumn:
                return QObject::tr("Address");
            case TestResultColumn:
                return QObject::tr("Test Result");
            case TrafficColumn:
                return QObject::tr("Traffic");
            default:
                return {};
        }
    }

    if (orientation == Qt::Vertical && section >= 0 && section < m_rowIds.size()) {
        const auto profile = ProxorGui::profileManager->GetProfile(profileIdAtRow(section));
        if (profile != nullptr && profile->id == ProxorGui::dataStore->started_id) return QStringLiteral("✓");
        return Int2String(section + 1);
    }

    return {};
}

Qt::ItemFlags ProxyListModel::flags(const QModelIndex &index) const {
    if (!index.isValid()) return Qt::NoItemFlags;

    auto out = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
    if (index.column() == ToggleColumn) {
        out |= Qt::ItemIsUserCheckable;
    }
    return out;
}

void ProxyListModel::setProfileIds(const QList<int> &ids) {
    beginResetModel();
    m_rowIds = ids;
    rebuildIndex();
    QSet<int> selectedIds;
    for (const auto id: m_selectedIds) {
        if (m_idToRow.contains(id)) selectedIds.insert(id);
    }
    m_selectedIds = selectedIds;
    endResetModel();
}

const QList<int> &ProxyListModel::profileIds() const {
    return m_rowIds;
}

QList<int> ProxyListModel::selectedProfileIds() const {
    QList<int> ids;
    for (const auto id: m_rowIds) {
        if (m_selectedIds.contains(id)) ids << id;
    }
    return ids;
}

bool ProxyListModel::isProfileSelected(int profileId) const {
    return m_selectedIds.contains(profileId);
}

void ProxyListModel::clearSelectedProfiles() {
    if (m_selectedIds.isEmpty()) return;
    const auto oldSelectedIds = m_selectedIds;
    m_selectedIds.clear();
    emitSelectionDataChanged(oldSelectedIds);
}

void ProxyListModel::setSelectedProfileIds(const QList<int> &ids) {
    const auto oldSelectedIds = m_selectedIds;
    m_selectedIds.clear();
    for (const auto id: ids) {
        if (m_idToRow.contains(id)) m_selectedIds.insert(id);
    }
    emitSelectionDataChanged(oldSelectedIds);
}

void ProxyListModel::selectOnlyProfile(int profileId) {
    setSelectedProfileIds({profileId});
}

void ProxyListModel::toggleSelectedProfile(int profileId) {
    if (!m_idToRow.contains(profileId)) return;
    const auto oldSelectedIds = m_selectedIds;
    if (m_selectedIds.contains(profileId)) {
        m_selectedIds.remove(profileId);
    } else {
        m_selectedIds.insert(profileId);
    }
    emitSelectionDataChanged(oldSelectedIds);
}

void ProxyListModel::selectProfileRange(int firstRow, int lastRow) {
    if (m_rowIds.isEmpty()) return;
    if (firstRow > lastRow) std::swap(firstRow, lastRow);
    const int lastAvailableRow = static_cast<int>(m_rowIds.size()) - 1;
    firstRow = std::max(0, firstRow);
    lastRow = std::min(lastRow, lastAvailableRow);

    QList<int> ids;
    for (int row = firstRow; row <= lastRow; ++row) {
        ids << m_rowIds[row];
    }
    setSelectedProfileIds(ids);
}

int ProxyListModel::profileIdAtRow(int row) const {
    if (row < 0 || row >= m_rowIds.size()) return -1;
    return m_rowIds[row];
}

int ProxyListModel::rowForProfile(int profileId) const {
    return m_idToRow.value(profileId, -1);
}

void ProxyListModel::refreshRows(const QSet<int> &ids) {
    if (m_rowIds.isEmpty()) return;

    if (ids.isEmpty()) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, ColumnCount - 1));
        emit headerDataChanged(Qt::Vertical, 0, rowCount() - 1);
        return;
    }

    for (const auto id: ids) {
        const int row = rowForProfile(id);
        if (row < 0) continue;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        emit headerDataChanged(Qt::Vertical, row, row);
    }
}

void ProxyListModel::moveProfileRow(int rowSrc, int rowDst) {
    if (rowSrc < 0 || rowDst < 0 || rowSrc >= m_rowIds.size() || rowDst >= m_rowIds.size() || rowSrc == rowDst) return;

    beginResetModel();
    const auto id = m_rowIds.takeAt(rowSrc);
    m_rowIds.insert(rowDst, id);
    rebuildIndex();
    endResetModel();

    emit orderChanged(m_rowIds);
}

bool ProxyListModel::rowMatchesText(int row, const QString &text) const {
    if (text.isEmpty()) return true;
    if (row < 0 || row >= m_rowIds.size()) return false;

    for (int column = NameColumn; column < ColumnCount; ++column) {
        const auto value = data(index(row, column), Qt::DisplayRole).toString();
        if (value.contains(text, Qt::CaseInsensitive)) return true;
    }
    return false;
}

bool ProxyListModel::rowMatchesColumnText(int row, int column, const QString &text) const {
    if (text.isEmpty()) return true;
    if (row < 0 || row >= m_rowIds.size()) return false;
    const auto value = data(index(row, column), Qt::DisplayRole).toString();
    return value.contains(text, Qt::CaseInsensitive);
}

void ProxyListModel::rebuildIndex() {
    m_idToRow.clear();
    for (int row = 0; row < m_rowIds.size(); ++row) {
        m_idToRow.insert(m_rowIds[row], row);
    }
}

void ProxyListModel::emitSelectionDataChanged(const QSet<int> &oldSelectedIds) {
    auto changedIds = oldSelectedIds;
    changedIds.unite(m_selectedIds);
    for (const auto id: changedIds) {
        const int row = rowForProfile(id);
        if (row < 0) continue;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1), {Qt::BackgroundRole, Qt::ForegroundRole});
    }
}
