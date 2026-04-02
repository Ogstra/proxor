#pragma once

#include <QTableView>

class ProxyListModel;

class ProxyListView : public QTableView {
    Q_OBJECT

public:
    explicit ProxyListView(QWidget *parent = nullptr);

    [[nodiscard]] QList<int> selectedProfileIds() const;

    void setSearchText(const QString &text);

    [[nodiscard]] QString searchText() const;

    void reapplySearchFilter();

protected:
    void dropEvent(QDropEvent *event) override;

private:
    QString m_searchText;
};
