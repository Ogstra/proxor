// src/ui/mac/MacDialogs.cpp — see MacDialogs.h. Compiled on macOS only (PLATFORM_SOURCES).

#include "MacDialogs.h"
#include "MacLookCommon.h"

#include <memory>

#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSizePolicy>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

namespace {

using ProxorMac::kHeaderQss;
using ProxorMac::kLineEditQss;
using ProxorMac::systemThemeActive;

const QString kScrollBarQss = QStringLiteral(
    "QScrollBar:vertical { background: transparent; width: 12px; margin: 0; }"
    "QScrollBar::handle:vertical { background: rgba(128,128,128,120); border-radius: 4px;"
    "  min-height: 32px; margin: 2px 2px; }"
    "QScrollBar::handle:vertical:hover { background: rgba(128,128,128,170); }"
    "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }");


constexpr auto kPolishedProp = "proxorMacPolished";

// Rounded, bordered fields that match the rest of the System theme. Combo box editors and spin boxes
// keep their native drawing.
void styleLineEdits(QWidget *root, bool sys) {
    for (auto *le : root->findChildren<QLineEdit *>()) {
        if (qobject_cast<QComboBox *>(le->parentWidget()) || qobject_cast<QAbstractSpinBox *>(le->parentWidget())) continue;
        if (le->property("proxorMacKeepStyle").toBool()) continue;
        if (sys) {
            if (le->styleSheet().isEmpty()) le->setStyleSheet(kLineEditQss);
        } else if (le->styleSheet() == kLineEditQss) {
            le->setStyleSheet(QString());
        }
    }
}

void centerOverParent(QDialog *d) {
    if (!d->isWindow()) return;
    QWidget *pw = d->parentWidget() ? d->parentWidget()->window() : nullptr;
    QRect area;
    if (pw != nullptr && pw->isVisible() && !pw->isMinimized()) {
        area = pw->frameGeometry();
    } else if (auto *screen = d->screen()) {
        area = screen->availableGeometry();
    } else {
        return;
    }
    QRect r = d->frameGeometry();
    r.moveCenter(area.center());
    if (auto *screen = d->screen()) {
        const QRect avail = screen->availableGeometry();
        r.moveLeft(qBound(avail.left(), r.left(), qMax(avail.left(), avail.right() - r.width())));
        r.moveTop(qBound(avail.top(), r.top(), qMax(avail.top(), avail.bottom() - r.height())));
    }
    d->move(r.topLeft());
}

void polishBasicSettings(QDialog *d) {
    // The page content is taller than the old default; give it room so less needs scrolling.
    d->resize(qMax(d->width(), 880), qMax(d->height(), 720));

    // Overlay scroll bars are invisible until you scroll, so the cut-off pages gave no hint that
    // there is more. A thin always-visible handle fixes that.
    for (auto *area : d->findChildren<QScrollArea *>()) {
        if (area->parentWidget() == d) area->verticalScrollBar()->setStyleSheet(kScrollBarQss);
    }

    // Custom cores (hysteria2 / naive / tuic): each row is "label | path | Select" in its own widget,
    // so the path fields started wherever each name ended. Give the names one shared width.
    QHash<QWidget *, QList<QLabel *>> coreRows;
    for (auto *row : d->findChildren<QWidget *>()) {
        auto *h = qobject_cast<QHBoxLayout *>(row->layout());
        if (h == nullptr || h->count() != 3) continue;
        auto *label = qobject_cast<QLabel *>(h->itemAt(0)->widget());
        if (label && qobject_cast<QLineEdit *>(h->itemAt(1)->widget()) &&
            qobject_cast<QPushButton *>(h->itemAt(2)->widget())) {
            coreRows[row->parentWidget()] << label;
        }
    }
    for (const auto &labels : coreRows) {
        int w = 0;
        for (auto *l : labels) w = qMax(w, l->sizeHint().width());
        for (auto *l : labels) l->setMinimumWidth(w);
    }

    // The lock button next to Listen Address: an icon-sized button, not a full-size push button.
    if (auto *auth = d->findChild<QPushButton *>(QStringLiteral("inbound_auth"))) {
        auth->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auth->setFixedWidth(40);
    }
}

// Edit dialogs stack several group boxes, each with its own "label | field" grid, so every section's
// fields started at a different x (the macOS font makes the label widths differ more than elsewhere).
// Give the first-column labels of all those grids one shared width so the fields line up down the
// whole dialog. Grids inside a group box nested in another one (e.g. Edit Group's "Update") are an
// indented sub-section and keep their own column.
void syncLabelColumns(QWidget *root) {
    QList<QLabel *> labels;
    for (auto *grid : root->findChildren<QGridLayout *>()) {
        QWidget *owner = grid->parentWidget();
        bool nested = false;
        for (QWidget *w = owner; w && w != root; w = w->parentWidget()) {
            if (qobject_cast<QGroupBox *>(w) && w != owner && qobject_cast<QGroupBox *>(owner)) nested = true;
        }
        if (nested) continue;
        for (int r = 0; r < grid->rowCount(); ++r) {
            auto *item = grid->itemAtPosition(r, 0);
            auto *l = item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
            if (l && !l->wordWrap() && !l->text().isEmpty() && grid->itemAtPosition(r, 1)) labels << l;
        }
    }
    int w = 0;
    for (auto *l : labels) w = qMax(w, l->sizeHint().width());
    for (auto *l : labels) l->setMinimumWidth(w);
}

void polishVpnPage(QWidget *page) {
    // TUN is not available on macOS yet: keep the page (its values are still saved), but disabled.
    page->setEnabled(false);
    page->setToolTip(QObject::tr("Not available on macOS yet"));
    // The vertical Line widget renders as a stray "[" with the macOS style.
    for (auto *line : page->findChildren<QFrame *>(QStringLiteral("line"))) line->hide();
    // "Pick..." was placed by absolute geometry over the group box's title, overlapping its border.
    // Put it into the group box layout, under the text box.
    auto *pick = page->findChild<QPushButton *>(QStringLiteral("btn_pick_process"));
    auto *box = page->findChild<QGroupBox *>(QStringLiteral("gb_process_name"));
    if (pick && box && box->layout() && !pick->property("proxorMacMoved").toBool()) {
        pick->setProperty("proxorMacMoved", true);
        auto *inner = new QHBoxLayout;
        inner->addStretch(1);
        inner->addWidget(pick);
        pick->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        if (auto *v = qobject_cast<QBoxLayout *>(box->layout())) v->addLayout(inner);
    }
}

void polishDialog(QDialog *d) {
    const QString cls = QString::fromLatin1(d->metaObject()->className());
    const bool sys = systemThemeActive();
    const bool first = !d->property(kPolishedProp).toBool();
    d->setProperty(kPolishedProp, true);

    // Same layout as on Windows and Linux (the .ui files are shared): only field drawing changes.
    styleLineEdits(d, sys);
    if (cls == QLatin1String("DialogEditProfile") || cls == QLatin1String("DialogEditGroup")) syncLabelColumns(d);

    if (!first) return;

    if (cls == QLatin1String("DialogBasicSettings")) polishBasicSettings(d);
    if (cls == QLatin1String("DialogVPNSettings")) polishVpnPage(d);
    if (cls == QLatin1String("DialogEditProfile") || cls == QLatin1String("DialogEditGroup")) {
        d->setMinimumWidth(qMax(d->minimumWidth(), 540));
        d->adjustSize();
    }
    if (cls == QLatin1String("DialogManageGroups")) {
        d->resize(qMax(d->width(), 780), qMax(d->height(), 420));
        if (auto *table = d->findChild<QTableView *>(QStringLiteral("listView"))) {
            // Framed, native header with centered captions, like the main window tables.
            table->horizontalHeader()->setStyleSheet(QString());
            table->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
            table->setAlternatingRowColors(true);
            table->setShowGrid(false);
            table->setFrameShape(QFrame::StyledPanel);
        }
    }

    // Centered over the main window instead of wherever the window server dropped it.
    QTimer::singleShot(0, d, [d = QPointer<QDialog>(d)] {
        if (d) centerOverParent(d);
    });
}

class DialogPolisher final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() != QEvent::Show) return false;
        auto *w = qobject_cast<QWidget *>(watched);
        if (w == nullptr) return false;
        if (auto *d = qobject_cast<QDialog *>(w)) {
            // Proxor's own dialogs are named Dialog*; Qt's (QMessageBox, QFileDialog...) keep their native look.
            if (QString::fromLatin1(d->metaObject()->className()).startsWith(QLatin1String("Dialog"))) polishDialog(d);
            return false;
        }
        // Tab pages of the (already polished) settings dialogs are shown lazily: align them when they appear.
        if (qobject_cast<QStackedWidget *>(w->parentWidget())) {
            if (auto *top = w->window(); top && qobject_cast<QDialog *>(top)) {
                if (QString::fromLatin1(top->metaObject()->className()).startsWith(QLatin1String("Dialog"))) {
                    QTimer::singleShot(0, w, [page = QPointer<QWidget>(w)] {
                        if (!page) return;
                        styleLineEdits(page, systemThemeActive());
                    });
                }
            }
        }
        return false;
    }
};

} // namespace

namespace ProxorMac {

void InstallDialogPolish() {
    static bool installed = false;
    if (installed || qApp == nullptr) return;
    installed = true;
    qApp->installEventFilter(new DialogPolisher(qApp));
}

} // namespace ProxorMac
