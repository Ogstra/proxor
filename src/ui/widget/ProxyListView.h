#pragma once

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

    void reapplySearchFilter();

protected:
    void initViewItemOption(QStyleOptionViewItem *option) const override;

    void mousePressEvent(QMouseEvent *event) override;

    void mouseDoubleClickEvent(QMouseEvent *event) override;

    void mouseReleaseEvent(QMouseEvent *event) override;

    void dropEvent(QDropEvent *event) override;

private:
    QString m_searchText;
    int m_anchorRow = -1;
    int m_lastClickedProfileId = -1;

    [[nodiscard]] ProxyListModel *proxyModel() const;

    bool handleProfileClick(QMouseEvent *event);

    [[nodiscard]] int profileIdAtMouseEvent(const QMouseEvent *event) const;

    void clearNativeCurrentCell();
};
