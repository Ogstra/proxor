#pragma once

#include <QHeaderView>

class ProxyListVerticalHeader : public QHeaderView {
    Q_OBJECT

public:
    explicit ProxyListVerticalHeader(QWidget *parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override;
};
