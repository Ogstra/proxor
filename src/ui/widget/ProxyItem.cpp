#include "ProxyItem.h"
#include "ui_ProxyItem.h"

#include <QEvent>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>

#include "ui/Icon.hpp"

ProxyItem::ProxyItem(QWidget *parent, const std::shared_ptr<ProxorGui::ProxyEntity> &ent, QListWidgetItem *item)
    : QWidget(parent), ui(new Ui::ProxyItem) {
    ui->setupUi(this);
    this->setLayoutDirection(Qt::LeftToRight);
    applyThemeStyle();

    this->item = item;
    this->ent = ent;
    if (ent == nullptr) return;

    refresh_data();
}

ProxyItem::~ProxyItem() {
    delete ui;
}

void ProxyItem::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        applyThemeStyle();
    }
}

void ProxyItem::applyThemeStyle() {
    // Palette roles instead of fixed gray/pink: readable on light and dark themes.
    auto setLabelColor = [](QLabel *label, const QColor &color) {
        QPalette pal = label->palette();
        if (pal.color(QPalette::WindowText) == color) return;
        pal.setColor(QPalette::WindowText, color);
        label->setPalette(pal);
    };
    setLabelColor(ui->address, palette().color(QPalette::Disabled, QPalette::Text));
    setLabelColor(ui->type, palette().color(QPalette::Link));

    // The material SVGs are black; Icon tints them with the current palette text color.
    ui->change->setIcon(QIcon(Icon::GetMaterialIcon("swap-horizontal")));
    ui->remove->setIcon(QIcon(Icon::GetMaterialIcon("delete")));
}

void ProxyItem::refresh_data() {
    ui->type->setText(ent->DisplayTypeSummary());
    ui->name->setText(ent->DisplayNameSummary());
    ui->address->setText(ent->DisplayAddressSummary());
    ui->traffic->setText(ent->traffic_data->DisplayTraffic());
    ui->test_result->setText(ent->DisplayLatency());

    runOnUiThread(
        [=] {
            adjustSize();
            item->setSizeHint(sizeHint());
            dynamic_cast<QWidget *>(parent())->adjustSize();
        },
        this);
}

void ProxyItem::on_remove_clicked() {
    if (!this->remove_confirm ||
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1?").arg(ent->DisplayNameSummary())) == QMessageBox::StandardButton::Yes) {
        // TODO do remove (or not) -> callback
        delete item;
    }
}

QPushButton *ProxyItem::get_change_button() {
    return ui->change;
}
