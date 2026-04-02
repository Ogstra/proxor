#include "ProxyListView.h"

#include <QDropEvent>
#include <QItemSelectionModel>

#include "ui/model/ProxyListModel.h"

ProxyListView::ProxyListView(QWidget *parent) : QTableView(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setDropIndicatorShown(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropOverwriteMode(false);
    setDragEnabled(true);
    setAcceptDrops(true);
}

QList<int> ProxyListView::selectedProfileIds() const {
    QList<int> ids;
    const auto rows = selectionModel() ? selectionModel()->selectedRows() : QModelIndexList{};
    for (const auto &index: rows) {
        const int id = index.data(ProxyListModel::ProfileIdRole).toInt();
        if (id >= 0 && !ids.contains(id)) ids << id;
    }
    return ids;
}

void ProxyListView::setSearchText(const QString &text) {
    m_searchText = text;
    reapplySearchFilter();
}

QString ProxyListView::searchText() const {
    return m_searchText;
}

void ProxyListView::reapplySearchFilter() {
    auto *proxyModel = qobject_cast<ProxyListModel *>(model());
    if (proxyModel == nullptr) return;

    for (int row = 0; row < proxyModel->rowCount(); ++row) {
        setRowHidden(row, !proxyModel->rowMatchesText(row, m_searchText));
    }
}

void ProxyListView::dropEvent(QDropEvent *event) {
    auto *proxyModel = qobject_cast<ProxyListModel *>(model());
    if (proxyModel == nullptr) {
        QTableView::dropEvent(event);
        return;
    }

    const auto current = currentIndex();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto target = indexAt(event->position().toPoint());
#else
    const auto target = indexAt(event->pos());
#endif
    if (!current.isValid() || !target.isValid()) {
        event->ignore();
        return;
    }

    proxyModel->moveProfileRow(current.row(), target.row());
    clearSelection();
    selectRow(target.row());
    reapplySearchFilter();
    event->acceptProposedAction();
}
