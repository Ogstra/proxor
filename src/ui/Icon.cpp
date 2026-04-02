#include "Icon.hpp"

#include "main/ProxorGui.hpp"

#include <QPainter>

namespace {
void drawWifiBadge(QPainter &painter, const QRect &rect) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::white, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QPoint center(rect.center().x(), rect.bottom() - 2);
    painter.drawArc(QRect(center.x() - 8, center.y() - 8, 16, 16), 30 * 16, 120 * 16);
    painter.drawArc(QRect(center.x() - 5, center.y() - 5, 10, 10), 30 * 16, 120 * 16);
    painter.drawArc(QRect(center.x() - 2, center.y() - 2, 4, 4), 30 * 16, 120 * 16);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPoint(center.x(), center.y()), 1, 1);
    painter.restore();
}

void drawEthernetBadge(QPainter &painter, const QRect &rect) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(Qt::white, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);

    const QRect plug(rect.left() + 3, rect.top() + 2, rect.width() - 6, rect.height() - 7);
    painter.drawRoundedRect(plug, 1.5, 1.5);
    const int y = plug.bottom() + 1;
    painter.drawLine(plug.center().x() - 3, y, plug.center().x() - 3, rect.bottom() - 1);
    painter.drawLine(plug.center().x(), y, plug.center().x(), rect.bottom() - 1);
    painter.drawLine(plug.center().x() + 3, y, plug.center().x() + 3, rect.bottom() - 1);
    painter.restore();
}

QPixmap withNetworkBadge(const QPixmap &base, bool wifi) {
    QPixmap pixmap = base;
    if (pixmap.isNull()) return pixmap;

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const int badgeSize = qMax(10, qMin(pixmap.width(), pixmap.height()) / 2);
    const QRect badgeRect(pixmap.width() - badgeSize - 1, pixmap.height() - badgeSize - 1, badgeSize, badgeSize);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 170));
    painter.drawEllipse(badgeRect);
    if (wifi) {
        drawWifiBadge(painter, badgeRect);
    } else {
        drawEthernetBadge(painter, badgeRect);
    }
    return pixmap;
}
}

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
    auto active = QPixmap(":/proxor/proxor_active.png");
    if (status == TrayIconStatus::ACTIVE_WIFI) return withNetworkBadge(active, true);
    if (status == TrayIconStatus::ACTIVE_ETHERNET) return withNetworkBadge(active, false);
    return active;
}

QPixmap Icon::GetMaterialIcon(const QString &name) {
    QPixmap pixmap(":/icon/material/" + name + ".svg");
    return pixmap;
}
