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

QLayout *owningLayout(QLayout *root, QWidget *w) {
    if (root == nullptr) return nullptr;
    if (root->indexOf(w) >= 0) return root;
    for (int i = 0; i < root->count(); ++i) {
        if (auto *child = root->itemAt(i)->layout()) {
            if (auto *hit = owningLayout(child, w)) return hit;
        }
    }
    return nullptr;
}

QLayout *owningLayout(QWidget *w) {
    for (QWidget *parent = w->parentWidget(); parent != nullptr; parent = parent->parentWidget()) {
        if (parent->layout()) {
            if (auto *hit = owningLayout(parent->layout(), w)) return hit;
        }
        if (qobject_cast<QDialog *>(parent) && parent->isWindow()) break;
    }
    return nullptr;
}

bool layoutHasFlexibleItem(QLayout *layout) {
    for (int i = 0; i < layout->count(); ++i) {
        auto *item = layout->itemAt(i);
        if (item->spacerItem()) {
            if (item->expandingDirections() & Qt::Horizontal) return true;
            continue;
        }
        if (item->expandingDirections() & Qt::Horizontal) return true;
    }
    return false;
}

struct FormScope {
    QList<QLabel *> labels;
};

// Collects the "label + field" rows below `layout`. Group boxes that sit side by side in one row
// (e.g. Inbound | Custom Inbound) are separate scopes: they are too narrow to share a label column
// with the full-width groups around them.
void collectFormLabels(QLayout *layout, FormScope &cur, QList<FormScope> &done) {
    if (layout == nullptr) return;
    auto isRowLabel = [](QLayoutItem *item) {
        auto *l = item ? qobject_cast<QLabel *>(item->widget()) : nullptr;
        return (l && !l->isHidden() && !l->wordWrap() && !l->text().isEmpty()) ? l : nullptr;
    };
    if (auto *grid = qobject_cast<QGridLayout *>(layout)) {
        for (int r = 0; r < grid->rowCount(); ++r) {
            if (auto *l = isRowLabel(grid->itemAtPosition(r, 0))) {
                if (grid->itemAtPosition(r, 1)) cur.labels << l;
            }
        }
    } else if (auto *hbox = qobject_cast<QHBoxLayout *>(layout)) {
        if (hbox->count() >= 2) {
            if (auto *l = isRowLabel(hbox->itemAt(0))) cur.labels << l;
        }
    }
    int sideBySide = 0;
    if (qobject_cast<QHBoxLayout *>(layout)) {
        for (int i = 0; i < layout->count(); ++i) {
            if (qobject_cast<QGroupBox *>(layout->itemAt(i)->widget())) ++sideBySide;
        }
    }
    for (int i = 0; i < layout->count(); ++i) {
        auto *item = layout->itemAt(i);
        if (auto *child = item->layout()) {
            collectFormLabels(child, cur, done);
        } else if (auto *w = item->widget()) {
            if (!w->layout() || qobject_cast<QScrollArea *>(w)) continue;
            if (sideBySide >= 2 && qobject_cast<QGroupBox *>(w)) {
                FormScope own;
                collectFormLabels(w->layout(), own, done);
                done << own;
            } else {
                collectFormLabels(w->layout(), cur, done);
            }
        }
    }
}

// Labels of every "label + field" row inside `root` get one common width and right alignment, so the
// fields start at the same x (the macOS form convention). Idempotent.
void alignFormColumns(QWidget *root) {
    FormScope top;
    QList<FormScope> scopes;
    collectFormLabels(root->layout(), top, scopes);
    scopes << top;
    for (const auto &scope : scopes) {
        if (scope.labels.isEmpty()) continue;
        int common = 0;
        for (auto *l : scope.labels) common = qMax(common, l->sizeHint().width());
        common = qMin(common, 280);
        for (auto *l : scope.labels) {
            if (scope.labels.size() > 1 && l->sizeHint().width() <= common) l->setMinimumWidth(common);
            l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        }
    }
}

// Rows that are laid out top-down should hug the top instead of spreading out over spare height.
void addBottomStretch(QLayout *layout) {
    auto *vbox = qobject_cast<QVBoxLayout *>(layout);
    if (vbox == nullptr || vbox->property("proxorMacStretch").toBool()) return;
    for (int i = 0; i < vbox->count(); ++i) {
        if (vbox->itemAt(i)->expandingDirections() & Qt::Vertical) return;
    }
    vbox->setProperty("proxorMacStretch", true);
    vbox->addStretch(1);
}

void tidyLayouts(QWidget *root) {
    if (root->layout()) addBottomStretch(root->layout());
    for (auto *box : root->findChildren<QGroupBox *>()) {
        if (auto *l = box->layout()) {
            if (!box->property("proxorMacMargins").toBool()) {
                box->setProperty("proxorMacMargins", true);
                l->setContentsMargins(12, 10, 12, 12);
            }
            addBottomStretch(l);
        }
    }
}

// Buttons keep their natural size (left aligned) instead of stretching across the whole row.
void tidyButtons(QWidget *root) {
    for (auto *btn : root->findChildren<QPushButton *>()) {
        if (qobject_cast<QDialogButtonBox *>(btn->parentWidget())) continue;
        if (btn->isFlat()) continue;
        const auto pol = btn->sizePolicy().horizontalPolicy();
        if (pol == QSizePolicy::Fixed || pol == QSizePolicy::Maximum) continue;
        auto *layout = owningLayout(btn);
        if (layout == nullptr) continue;
        btn->setSizePolicy(QSizePolicy::Maximum, btn->sizePolicy().verticalPolicy());
        if (qobject_cast<QVBoxLayout *>(layout) || qobject_cast<QGridLayout *>(layout)) {
            layout->setAlignment(btn, Qt::AlignLeft);
        } else if (auto *hbox = qobject_cast<QHBoxLayout *>(layout)) {
            if (!layoutHasFlexibleItem(hbox)) hbox->addStretch(1);
        }
    }
}

// Rounded, bordered fields that match the rest of the System theme. Combo box editors and spin boxes
// keep their native drawing.
void styleLineEdits(QWidget *root, bool sys) {
    for (auto *le : root->findChildren<QLineEdit *>()) {
        if (qobject_cast<QComboBox *>(le->parentWidget()) || qobject_cast<QAbstractSpinBox *>(le->parentWidget())) continue;
        if (le->property("proxorMacKeepStyle").toBool()) continue;
        // Fields in the same column must reach the same right edge: some .ui line edits are Preferred
        // or Ignored, which stops them a few points short of their cell (or collapses them).
        if (le->minimumWidth() != le->maximumWidth() && le->sizePolicy().horizontalPolicy() != QSizePolicy::Expanding) {
            le->setSizePolicy(QSizePolicy::Expanding, le->sizePolicy().verticalPolicy());
        }
        if (sys) {
            if (le->styleSheet().isEmpty()) le->setStyleSheet(kLineEditQss);
        } else if (le->styleSheet() == kLineEditQss) {
            le->setStyleSheet(QString());
        }
    }
}

void limitWidth(QWidget *root, std::initializer_list<const char *> names, int width) {
    for (const char *name : names) {
        auto *w = root->findChild<QWidget *>(QString::fromLatin1(name));
        if (w == nullptr) continue;
        // Some of these edits have an Ignored size policy in the .ui, which collapses to zero next to a stretch.
        w->setFixedWidth(width);
        if (auto *box = qobject_cast<QBoxLayout *>(owningLayout(w))) {
            if (!layoutHasFlexibleItem(box)) box->addStretch(1);
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

    limitWidth(d, {"inbound_socks_port", "test_concurrent", "test_download_timeout", "max_log_line",
                   "sub_auto_update", "mux_concurrency"}, 92);

    // The lock button next to Listen Address: an icon-sized button, not a full-size push button.
    if (auto *auth = d->findChild<QPushButton *>(QStringLiteral("inbound_auth"))) {
        auth->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auth->setFixedWidth(40);
    }

    // Subscriptions: "Interval (minute, invalid if less than 30)" broke the row. Give it its own row.
    auto *grid = d->findChild<QGridLayout *>(QStringLiteral("gridLayout_sub_auto"));
    auto *row = d->findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout_5"));
    auto *intervalLabel = d->findChild<QLabel *>(QStringLiteral("label_21"));
    auto *intervalEdit = d->findChild<QLineEdit *>(QStringLiteral("sub_auto_update"));
    auto *uaLabel = d->findChild<QLabel *>(QStringLiteral("label_4"));
    auto *uaEdit = d->findChild<QLineEdit *>(QStringLiteral("user_agent"));
    if (grid && row && intervalLabel && intervalEdit && uaLabel && uaEdit &&
        !d->property("proxorMacIntervalMoved").toBool()) {
        d->setProperty("proxorMacIntervalMoved", true);
        row->removeWidget(intervalLabel);
        row->removeWidget(intervalEdit);
        grid->removeWidget(uaLabel);
        grid->removeWidget(uaEdit);
        auto *intervalRow = new QHBoxLayout;
        intervalRow->setContentsMargins(0, 0, 0, 0);
        intervalRow->addWidget(intervalEdit);
        intervalRow->addStretch(1);
        grid->addWidget(intervalLabel, 1, 0);
        grid->addLayout(intervalRow, 1, 1);
        grid->addWidget(uaLabel, 2, 0);
        grid->addWidget(uaEdit, 2, 1);
        intervalEdit->setFixedWidth(92);
    }
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

    styleLineEdits(d, sys);
    tidyButtons(d);
    tidyLayouts(d);
    alignFormColumns(d);

    if (!first) return;

    if (cls == QLatin1String("DialogBasicSettings")) polishBasicSettings(d);
    if (cls == QLatin1String("DialogVPNSettings")) polishVpnPage(d);
    if (cls == QLatin1String("DialogEditProfile") || cls == QLatin1String("DialogEditGroup")) {
        d->setMinimumWidth(qMax(d->minimumWidth(), 540));
        d->adjustSize();
    }
    if (cls == QLatin1String("DialogEditGroup")) {
        // "Update" was a group box nested inside the "Subscription" group box.
        if (auto *inner = d->findChild<QGroupBox *>(QStringLiteral("cat_update"))) inner->setFlat(true);
    }
    if (cls == QLatin1String("DialogManageGroups")) {
        d->resize(qMax(d->width(), 780), qMax(d->height(), 420));
        if (auto *table = d->findChild<QTableView *>(QStringLiteral("listView"))) {
            table->horizontalHeader()->setStyleSheet(sys ? kHeaderQss : QString());
            table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            table->setAlternatingRowColors(true);
            table->setShowGrid(false);
            table->setFrameShape(QFrame::NoFrame);
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
                        tidyButtons(page);
                        tidyLayouts(page);
                        alignFormColumns(page);
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
