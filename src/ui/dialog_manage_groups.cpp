#include "dialog_manage_groups.h"
#include "ui_dialog_manage_groups.h"

#include "db/Database.hpp"
#include "sub/GroupUpdater.hpp"
#include "main/GuiUtils.hpp"
#include "ui/edit/dialog_edit_group.h"

#include <QHeaderView>
#include <QMessageBox>

DialogManageGroups::DialogManageGroups(QWidget *parent) : QDialog(parent), ui(new Ui::DialogManageGroups) {
    ui->setupUi(this);
    groupListModel = new GroupListModel(this);
    ui->listView->setModel(groupListModel);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::TypeColumn, QHeaderView::ResizeToContents);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::NameColumn, QHeaderView::Stretch);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::UrlColumn, QHeaderView::Stretch);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::InfoColumn, QHeaderView::Stretch);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::UpdateColumn, QHeaderView::ResizeToContents);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::EditColumn, QHeaderView::ResizeToContents);
    ui->listView->horizontalHeader()->setSectionResizeMode(GroupListModel::RemoveColumn, QHeaderView::ResizeToContents);
    ui->listView->verticalHeader()->setDefaultSectionSize(28);

    reload_groups();
    connect(ProxorGui_sub::groupUpdater, &ProxorGui_sub::GroupUpdater::asyncUpdateCallback, this, [=](int gid) {
        Q_UNUSED(gid)
        reload_groups();
    });
}

DialogManageGroups::~DialogManageGroups() {
    delete ui;
}

void DialogManageGroups::reload_groups() {
    groupListModel->reload();
}

void DialogManageGroups::edit_group(int groupId) {
    auto ent = ProxorGui::profileManager->GetGroup(groupId);
    if (ent == nullptr) return;

    auto dialog = new DialogEditGroup(ent, this);
    connect(dialog, &QDialog::finished, this, [=] {
        if (dialog->result() == QDialog::Accepted) {
            ProxorGui::profileManager->SaveGroup(ent);
            reload_groups();
            MW_dialog_message(Dialog_DialogManageGroups, "refresh" + Int2String(ent->id));
        }
        dialog->deleteLater();
    });
    dialog->show();
}

void DialogManageGroups::remove_group(int groupId) {
    auto ent = ProxorGui::profileManager->GetGroup(groupId);
    if (ent == nullptr || ProxorGui::profileManager->groups.size() <= 1) return;

    if (QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->name)) ==
        QMessageBox::StandardButton::Yes) {
        ProxorGui::profileManager->DeleteGroup(ent->id);
        reload_groups();
        MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
    }
}

void DialogManageGroups::update_group(int groupId) {
    auto ent = ProxorGui::profileManager->GetGroup(groupId);
    if (ent == nullptr || ent->url.isEmpty()) return;
    ProxorGui_sub::groupUpdater->AsyncUpdate(ent->url, ent->id);
}

void DialogManageGroups::on_add_clicked() {
    auto ent = ProxorGui::ProfileManager::NewGroup();
    auto dialog = new DialogEditGroup(ent, this);
    int ret = dialog->exec();
    dialog->deleteLater();

    if (ret == QDialog::Accepted) {
        ProxorGui::profileManager->AddGroup(ent);
        reload_groups();
        MW_dialog_message(Dialog_DialogManageGroups, "refresh-1");
        if (!ent->url.trimmed().isEmpty()) {
            ProxorGui_sub::groupUpdater->AsyncUpdate(ent->url, ent->id);
        }
    }
}

void DialogManageGroups::on_update_all_clicked() {
    if (QMessageBox::question(this, tr("Confirmation"), tr("Update all subscriptions?")) == QMessageBox::StandardButton::Yes) {
        UI_update_all_groups();
    }
}

void DialogManageGroups::on_listView_clicked(const QModelIndex &index) {
    if (!index.isValid()) return;
    const int groupId = index.data(GroupListModel::GroupIdRole).toInt();
    if (groupId < 0) return;

    if (index.column() == GroupListModel::UpdateColumn) {
        update_group(groupId);
    } else if (index.column() == GroupListModel::EditColumn) {
        edit_group(groupId);
    } else if (index.column() == GroupListModel::RemoveColumn) {
        remove_group(groupId);
    }
}

void DialogManageGroups::on_listView_doubleClicked(const QModelIndex &index) {
    if (!index.isValid()) return;
    const int groupId = index.data(GroupListModel::GroupIdRole).toInt();
    if (groupId >= 0) edit_group(groupId);
}
