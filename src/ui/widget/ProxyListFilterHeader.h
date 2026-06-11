#pragma once

#include <QAbstractItemView>
#include <QHeaderView>
#include <QLineEdit>
#include <QObject>
#include <QToolButton>

#include "ui/model/ProxyListModel.h"

class ProxyListFilterHeader : public QObject {
    Q_OBJECT
public:
    explicit ProxyListFilterHeader(QHeaderView *header, QObject *parent = nullptr)
        : QObject(parent), m_header(header),
          m_view(qobject_cast<QAbstractItemView *>(header->parentWidget())) {
        auto *vp = header->viewport();

        name_filter    = makeEditor(vp, tr("Filter..."));
        type_filter    = makeEditor(vp, tr("Filter..."));
        address_filter = makeEditor(vp, tr("Filter..."));
        test_filter    = makeEditor(vp, tr("Filter..."));

        auto wire = [this](QLineEdit *e, int col) {
            connect(e, &QLineEdit::textChanged, this, [this, col](const QString &text) {
                emit filterChanged(col, text);
            });
        };
        wire(name_filter,    ProxyListModel::NameColumn);
        wire(type_filter,    ProxyListModel::TypeColumn);
        wire(address_filter, ProxyListModel::AddressColumn);
        wire(test_filter,    ProxyListModel::TestResultColumn);

        connect(header, &QHeaderView::sectionResized,   this, &ProxyListFilterHeader::adjustPositions);
        connect(header, &QHeaderView::geometriesChanged, this, &ProxyListFilterHeader::adjustPositions);
    }

public slots:
    void setFiltersVisible(bool visible) {
        m_filtersVisible = visible;

        if (!visible) {
            name_filter->clear();
            type_filter->clear();
            address_filter->clear();
            test_filter->clear();
        }

        if (auto btn = qobject_cast<QToolButton *>(sender())) {
            btn->setToolTip(QString("%1\n%2").arg(
                visible ? tr("Disable Filter") : tr("Enable Filter"),
                QKeySequence(QKeySequence::Find).toString(QKeySequence::NativeText)));
        }

        const int baseH = m_header->sizeHint().height();
        m_header->setFixedHeight(visible ? baseH + 32 : baseH);
        if (m_view) QMetaObject::invokeMethod(m_view, "updateGeometries");
        m_header->setDefaultAlignment(visible ? (Qt::AlignLeft | Qt::AlignTop)
                                               : (Qt::AlignLeft | Qt::AlignVCenter));

        name_filter->setVisible(visible);
        type_filter->setVisible(visible);
        address_filter->setVisible(visible);
        test_filter->setVisible(visible);

        adjustPositions();
    }

signals:
    void filterChanged(int logicalSection, const QString &text);

private slots:
    void adjustPositions() {
        if (!m_filtersVisible) return;

        const int editHeight = 24;
        const int topPos = m_header->height() - editHeight - 4;

        auto place = [&](QLineEdit *e, int section) {
            e->setGeometry(m_header->sectionViewportPosition(section) + 2,
                           topPos,
                           m_header->sectionSize(section) - 4,
                           editHeight);
        };
        place(name_filter,    ProxyListModel::NameColumn);
        place(type_filter,    ProxyListModel::TypeColumn);
        place(address_filter, ProxyListModel::AddressColumn);
        place(test_filter,    ProxyListModel::TestResultColumn);
    }

private:
    static QLineEdit *makeEditor(QWidget *parent, const QString &placeholder) {
        auto *e = new QLineEdit(parent);
        e->setPlaceholderText(placeholder);
        e->setClearButtonEnabled(true);
        e->setVisible(false);
        return e;
    }

    QHeaderView *m_header;
    QAbstractItemView *m_view = nullptr;
    QLineEdit *name_filter    = nullptr;
    QLineEdit *type_filter    = nullptr;
    QLineEdit *address_filter = nullptr;
    QLineEdit *test_filter    = nullptr;
    bool m_filtersVisible = false;
};
