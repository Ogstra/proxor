#include "ProxyListView.h"

#include <QDropEvent>
#include <QEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QTimer>

#include "ui/model/ProxyListModel.h"

namespace {
bool isPlainLeftClick(const QMouseEvent *event) {
    return event != nullptr &&
           event->button() == Qt::LeftButton &&
           !(event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier));
}
}

ProxyListView::ProxyListView(QWidget *parent) : QTableView(parent) {
    setDragDropMode(QAbstractItemView::InternalMove);
    setDropIndicatorShown(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setDragDropOverwriteMode(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setIconSize(QSize(22, 16));
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

QItemSelectionModel::SelectionFlags ProxyListView::selectionCommand(const QModelIndex &index, const QEvent *event) const {
    if (event != nullptr && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick)) {
        auto *mouseEvent = static_cast<const QMouseEvent *>(event);
        if (isPlainLeftClick(mouseEvent) && index.isValid()) {
            return QItemSelectionModel::ClearAndSelect;
        }
    }
    return QTableView::selectionCommand(index, event);
}

void ProxyListView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) {
    QTableView::selectionChanged(selected, deselected);
    if (viewport() == nullptr) return;

    viewport()->update();
    viewport()->repaint();
    QTimer::singleShot(0, viewport(), [viewport = viewport()] {
        if (viewport == nullptr) return;
        viewport->update();
        viewport->repaint();
    });
}

void ProxyListView::currentChanged(const QModelIndex &current, const QModelIndex &previous) {
    QTableView::currentChanged(current, previous);
    if (viewport() != nullptr) {
        viewport()->update();
        viewport()->repaint();
    }
}

void ProxyListView::mousePressEvent(QMouseEvent *event) {
    if (isPlainLeftClick(event) && indexAt(event->pos()).isValid()) {
        clearSelection();
    }
    QTableView::mousePressEvent(event);
}

void ProxyListView::mouseDoubleClickEvent(QMouseEvent *event) {
    const auto index = indexAt(event->pos());
    if (isPlainLeftClick(event) && index.isValid()) {
        clearSelection();
        selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        setCurrentIndex(index);
        if (viewport() != nullptr) viewport()->repaint();
    }
    QTableView::mouseDoubleClickEvent(event);
}

void ProxyListView::mouseReleaseEvent(QMouseEvent *event) {
    QTableView::mouseReleaseEvent(event);
    if (viewport() != nullptr) {
        viewport()->update();
        viewport()->repaint();
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
