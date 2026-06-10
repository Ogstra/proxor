#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QList>
#include <QSet>

#include <memory>

namespace ProxorGui {
    class Group;
}

class ProxyListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ToggleColumn = 0,
        NameColumn,
        TypeColumn,
        AddressColumn,
        TestResultColumn,
        TrafficColumn,
        ColumnCount
    };

    static QString quotaText(const std::shared_ptr<ProxorGui::Group> &group);

    enum Role {
        ProfileIdRole = Qt::UserRole + 1
    };

    explicit ProxyListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    bool setData(const QModelIndex &index, const QVariant &value, int role) override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setProfileIds(const QList<int> &ids);

    [[nodiscard]] const QList<int> &profileIds() const;

    [[nodiscard]] QList<int> selectedProfileIds() const;

    [[nodiscard]] bool isProfileSelected(int profileId) const;

    void clearSelectedProfiles();

    void setSelectedProfileIds(const QList<int> &ids);

    void selectOnlyProfile(int profileId);

    void toggleSelectedProfile(int profileId);

    void selectProfileRange(int firstRow, int lastRow);

    [[nodiscard]] int profileIdAtRow(int row) const;

    [[nodiscard]] int rowForProfile(int profileId) const;

    void refreshRows(const QSet<int> &ids = {});

    void moveProfileRow(int rowSrc, int rowDst);

    [[nodiscard]] bool rowMatchesText(int row, const QString &text) const;

signals:
    void orderChanged(const QList<int> &order);

private:
    QList<int> m_rowIds;
    QHash<int, int> m_idToRow;
    QSet<int> m_selectedIds;

    void rebuildIndex();

    void emitSelectionDataChanged(const QSet<int> &oldSelectedIds);
};
