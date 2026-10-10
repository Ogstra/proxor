#include "Icon.hpp"

#include "main/ProxorGui.hpp"

#include <QGuiApplication>
#include <QIcon>
#include <QPainter>
#include <QPalette>



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

namespace {
// Rasterize an SVG so it stays sharp on HiDPI screens; the pixmap carries the device pixel ratio.
QPixmap renderSvg(const QString &resource, const QSize &logicalSize) {
    const QIcon icon(resource);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return icon.pixmap(logicalSize, qGuiApp->devicePixelRatio());
#else
    return icon.pixmap(logicalSize);
#endif
}
} // namespace

QPixmap Icon::GetMaterialIcon(const QString &name) {
    // The material SVGs have no fill (they render black), so tint them with the text color of the
    // active palette to keep them visible on dark themes.
    QPixmap pixmap = renderSvg(":/icon/material/" + name + ".svg", QSize(24, 24));
    if (pixmap.isNull()) return pixmap;
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), QGuiApplication::palette().color(QPalette::Text));
    painter.end();
    return pixmap;
}

QPixmap Icon::GetCountryFlag(const QString &countryCode) {
    if (countryCode.size() != 2) return {};
    return renderSvg(":/proxor/flags/4x3/" + countryCode.toLower() + ".svg", QSize(22, 16));
}
