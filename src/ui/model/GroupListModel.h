#pragma once

#include <QAbstractTableModel>
#include <QList>

class GroupListModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        TypeColumn = 0,
        NameColumn,
        UrlColumn,
        InfoColumn,
        UpdateColumn,
        EditColumn,
        RemoveColumn,
        ColumnCount
    };

    enum Role {
        GroupIdRole = Qt::UserRole + 1
    };

    explicit GroupListModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] int columnCount(const QModelIndex &parent = {}) const override;

    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    [[nodiscard]] QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    [[nodiscard]] Qt::ItemFlags flags(const QModelIndex &index) const override;

    void reload();

    [[nodiscard]] int groupIdAtRow(int row) const;

private:
    QList<int> m_groupIds;
};
