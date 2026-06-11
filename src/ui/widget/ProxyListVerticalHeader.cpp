#include "ProxyListVerticalHeader.h"

#include <QPainter>
#include <QStyleOptionHeader>

ProxyListVerticalHeader::ProxyListVerticalHeader(QWidget *parent)
    : QHeaderView(Qt::Vertical, parent) {
    setSectionResizeMode(QHeaderView::Fixed);
    connect(this, &QHeaderView::sectionCountChanged, this, [this](int, int) {
        updateGeometry();
    });
}

QSize ProxyListVerticalHeader::sizeHint() const {
    auto size = QHeaderView::sizeHint();
    const int rows = count();
    if (rows > 0) {
        const int w = fontMetrics().horizontalAdvance(QString::number(rows)) + 10;
        size.setWidth(qMax(w, 24));
    } else {
        size.setWidth(24);
    }
    return size;
}

void ProxyListVerticalHeader::paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const {
    if (!rect.isValid()) return;

    painter->save();

    QStyleOptionHeader opt;
    initStyleOption(&opt);
    opt.rect = rect;
    opt.section = logicalIndex;
    opt.orientation = Qt::Vertical;
    opt.position = QStyleOptionHeader::Middle;
    opt.state |= QStyle::State_Enabled;

    const auto text = model() ? model()->headerData(logicalIndex, Qt::Vertical, Qt::DisplayRole).toString() : QString{};
    opt.text = text;

    style()->drawControl(QStyle::CE_Header, &opt, painter, this);

    painter->restore();
}
