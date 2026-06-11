#pragma once

#include <QHash>
#include <QPoint>
#include <QTableView>

class ProxyListModel;
class QMouseEvent;
class QStyleOptionViewItem;

class ProxyListView : public QTableView {
    Q_OBJECT

public:
    explicit ProxyListView(QWidget *parent = nullptr);

    [[nodiscard]] QList<int> selectedProfileIds() const;

    void clearSelection();

    void selectAll() override;

    void setSearchText(const QString &text);

    [[nodiscard]] QString searchText() const;

    void setColumnFilter(int column, const QString &text);

    void reapplySearchFilter();

protected:
    void initViewItemOption(QStyleOptionViewItem *option) const override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void mouseMoveEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void paintEvent(QPaintEvent *event) override;

    void dropEvent(QDropEvent *event) override;

private:
    QString m_searchText;
    QHash<int, QString> m_columnFilters;
    int m_anchorRow = -1;
    int m_lastClickedProfileId = -1;
    int m_dragStartProfileId = -1;
    int m_dropIndicatorRow = -1;
    QPoint m_dragStartPos;

    [[nodiscard]] ProxyListModel *proxyModel() const;

    bool handleProfileClick(QMouseEvent *event);

    [[nodiscard]] int profileIdAtMouseEvent(const QMouseEvent *event) const;

    void beginProfileDragCandidate(const QMouseEvent *event);

    bool finishProfileDragCandidate(const QMouseEvent *event);

    [[nodiscard]] int insertionRowAtPosition(const QPoint &pos) const;

    [[nodiscard]] int indicatorYForInsertionRow(int insertionRow) const;

    void setDropIndicatorRow(int row);

    void clearNativeCurrentCell();
};
