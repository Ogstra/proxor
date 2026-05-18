#include "Icon.hpp"

#include "main/ProxorGui.hpp"



QPixmap Icon::GetTrayIcon(Icon::TrayIconStatus status) {
    QPixmap pixmap;

    // software embedded icon
    auto pixmap_read = QPixmap(":/proxor/" + software_name.toLower() + ".png");
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    // software pack icon
    pixmap_read = QPixmap(ProxorGui::PackageFilePath(software_name.toLower() + ".png"));
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    // user icon
    pixmap_read = QPixmap("./" + software_name.toLower() + ".png");
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    if (status == TrayIconStatus::NONE) return pixmap;
    return QPixmap(":/proxor/proxor_active.png");
}

QPixmap Icon::GetMaterialIcon(const QString &name) {
    QPixmap pixmap(":/icon/material/" + name + ".svg");
    return pixmap;
}

QPixmap Icon::GetCountryFlag(const QString &countryCode) {
    if (countryCode.size() != 2) return {};
    return QPixmap(":/proxor/flags/4x3/" + countryCode.toLower() + ".svg").scaled(
        22,
        16,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
}
