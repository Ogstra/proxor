#include "ProxyListView.h"

#include <QDropEvent>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionViewItem>

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
    setSelectionMode(QAbstractItemView::NoSelection);
    setDragDropOverwriteMode(false);
    setDragEnabled(true);
    setAcceptDrops(true);
    setIconSize(QSize(22, 16));
}

QList<int> ProxyListView::selectedProfileIds() const {
    auto *proxy = proxyModel();
    return proxy == nullptr ? QList<int>{} : proxy->selectedProfileIds();
}

void ProxyListView::clearSelection() {
    if (auto *proxy = proxyModel()) proxy->clearSelectedProfiles();
    QTableView::clearSelection();
    m_anchorRow = -1;
    if (viewport() != nullptr) viewport()->update();
}

void ProxyListView::selectAll() {
    auto *proxy = proxyModel();
    if (proxy == nullptr) return;

    QList<int> ids;
    int anchorRow = -1;
    for (int row = 0; row < proxy->rowCount(); ++row) {
        if (isRowHidden(row)) continue;
        ids << proxy->profileIdAtRow(row);
        if (anchorRow < 0) anchorRow = row;
    }
    proxy->setSelectedProfileIds(ids);
    m_anchorRow = anchorRow;
    if (viewport() != nullptr) viewport()->update();
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

void ProxyListView::initViewItemOption(QStyleOptionViewItem *option) const {
    QTableView::initViewItemOption(option);
    if (option == nullptr) return;

    option->state &= ~(QStyle::State_MouseOver |
                       QStyle::State_HasFocus |
                       QStyle::State_Selected |
                       QStyle::State_Sunken);
}

void ProxyListView::mousePressEvent(QMouseEvent *event) {
    if (handleProfileClick(event)) {
        event->accept();
        clearNativeCurrentCell();
        return;
    }
    QTableView::mousePressEvent(event);
    clearNativeCurrentCell();
}

void ProxyListView::mouseDoubleClickEvent(QMouseEvent *event) {
    const auto index = event == nullptr ? QModelIndex{} : indexAt(event->pos());
    if (index.isValid() && index.column() == ProxyListModel::ToggleColumn) {
        event->accept();
        clearNativeCurrentCell();
        return;
    }
    if (isPlainLeftClick(event)) {
        const int profileId = profileIdAtMouseEvent(event);
        if (profileId >= 0 && profileId != m_lastClickedProfileId) {
            event->accept();
            clearNativeCurrentCell();
            return;
        }
    }
    const bool handled = handleProfileClick(event);
    if (handled && index.isValid()) {
        emit doubleClicked(index);
        event->accept();
        clearNativeCurrentCell();
        return;
    }
    QTableView::mouseDoubleClickEvent(event);
    clearNativeCurrentCell();
}

void ProxyListView::mouseReleaseEvent(QMouseEvent *event) {
    if (event != nullptr && event->button() == Qt::LeftButton) {
        m_lastClickedProfileId = profileIdAtMouseEvent(event);
        event->accept();
        clearNativeCurrentCell();
        return;
    }
    QTableView::mouseReleaseEvent(event);
    clearNativeCurrentCell();
    if (viewport() != nullptr) {
        viewport()->update();
    }
}

void ProxyListView::dropEvent(QDropEvent *event) {
    auto *proxy = proxyModel();
    if (proxy == nullptr) {
        QTableView::dropEvent(event);
        return;
    }

    const auto current = currentIndex();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto target = indexAt(event->position().toPoint());
#else
    const auto target = indexAt(event->pos());
#endif
    const int sourceRow = current.isValid() ? current.row() : m_anchorRow;
    if (sourceRow < 0 || !target.isValid()) {
        event->ignore();
        return;
    }

    const int movedProfileId = proxy->profileIdAtRow(sourceRow);
    proxy->moveProfileRow(sourceRow, target.row());
    proxy->selectOnlyProfile(movedProfileId);
    m_anchorRow = proxy->rowForProfile(movedProfileId);
    reapplySearchFilter();
    event->acceptProposedAction();
}

ProxyListModel *ProxyListView::proxyModel() const {
    return qobject_cast<ProxyListModel *>(model());
}

bool ProxyListView::handleProfileClick(QMouseEvent *event) {
    if (event == nullptr || event->button() != Qt::LeftButton) return false;

    auto *proxy = proxyModel();
    if (proxy == nullptr) return false;

    const auto index = indexAt(event->pos());
    if (!index.isValid()) return false;

    const int row = index.row();
    const int profileId = proxy->profileIdAtRow(row);
    if (profileId < 0) return false;

    if (index.column() == ProxyListModel::ToggleColumn) {
        const auto currentState = static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt());
        proxy->setData(index, currentState == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
        return true;
    }

    if (event->modifiers() & Qt::ShiftModifier) {
        const int firstRow = m_anchorRow >= 0 ? m_anchorRow : row;
        proxy->selectProfileRange(firstRow, row);
    } else if (event->modifiers() & Qt::ControlModifier) {
        proxy->toggleSelectedProfile(profileId);
        m_anchorRow = row;
    } else {
        proxy->selectOnlyProfile(profileId);
        m_anchorRow = row;
    }

    setCurrentIndex(index);
    if (viewport() != nullptr) viewport()->update();
    return true;
}

int ProxyListView::profileIdAtMouseEvent(const QMouseEvent *event) const {
    if (event == nullptr) return -1;

    auto *proxy = proxyModel();
    if (proxy == nullptr) return -1;

    const auto index = indexAt(event->pos());
    if (!index.isValid()) return -1;

    return proxy->profileIdAtRow(index.row());
}

void ProxyListView::clearNativeCurrentCell() {
    if (selectionModel() != nullptr) {
        selectionModel()->clearCurrentIndex();
    }
    QTableView::setCurrentIndex(QModelIndex());
    if (viewport() != nullptr) viewport()->update();
}
