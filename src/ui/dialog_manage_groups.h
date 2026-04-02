#pragma once

#include <QWidget>
#include <QDialog>
#include <QModelIndex>

#include "ui/model/GroupListModel.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogManageGroups;
}
QT_END_NAMESPACE

class DialogManageGroups : public QDialog {
    Q_OBJECT

public:
    explicit DialogManageGroups(QWidget *parent = nullptr);

    ~DialogManageGroups() override;

private:
    Ui::DialogManageGroups *ui;
    GroupListModel *groupListModel = nullptr;

    void reload_groups();

    void edit_group(int groupId);

    void remove_group(int groupId);

    void update_group(int groupId);

private slots:

    void on_add_clicked();

    void on_update_all_clicked();

    void on_listView_clicked(const QModelIndex &index);

    void on_listView_doubleClicked(const QModelIndex &index);
};
