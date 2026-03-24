#include "edit_shadowsocks.h"
#include "ui_edit_shadowsocks.h"

#include "fmt/ShadowSocksBean.hpp"
#include "fmt/Preset.hpp"

EditShadowSocks::EditShadowSocks(QWidget *parent) : QWidget(parent),
                                                    ui(new Ui::EditShadowSocks) {
    ui->setupUi(this);
    ui->method->addItems(Preset::SingBox::ShadowsocksMethods);
}

EditShadowSocks::~EditShadowSocks() {
    delete ui;
}

void EditShadowSocks::onStart(std::shared_ptr<ProxorGui::ProxyEntity> _ent) {
    this->ent = _ent;
    auto bean = this->ent->ShadowSocksBean();

    ui->method->setCurrentText(bean->method);
    ui->uot->setCurrentIndex(bean->uot);
    ui->password->setText(bean->password);
    ui->plugin->setCurrentText(bean->PluginName());
    ui->plugin_opts->setText(bean->PluginOptions());
}

bool EditShadowSocks::onEnd() {
    auto bean = this->ent->ShadowSocksBean();

    bean->method = ui->method->currentText();
    bean->password = ui->password->text();
    bean->uot = ui->uot->currentIndex();
    bean->SetPluginParts(ui->plugin->currentText(), ui->plugin_opts->text());

    return true;
}
