#pragma once

#include <QItemSelectionModel>
#include <QTableView>

class ProxyListModel;
class QEvent;
class QItemSelection;
class QMouseEvent;

class ProxyListView : public QTableView {
    Q_OBJECT

public:
    explicit ProxyListView(QWidget *parent = nullptr);

    [[nodiscard]] QList<int> selectedProfileIds() const;

    void setSearchText(const QString &text);

    [[nodiscard]] QString searchText() const;

    void reapplySearchFilter();

protected:
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex &index, const QEvent *event = nullptr) const override;

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void dropEvent(QDropEvent *event) override;

private:
    QString m_searchText;
};
