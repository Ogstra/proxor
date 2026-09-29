// src/ui/mac/MacLook.cpp — see MacLook.h. Compiled on macOS only (PLATFORM_SOURCES).

#include "MacLook.h"
#include "MacPlatform.h"

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

const QString kTabBarQss = QStringLiteral(
    "QTabBar { background: transparent; }"
    "QTabBar::tab { background: transparent; color: palette(window-text); border: none;"
    "  border-radius: 6px; padding: 3px 12px; margin: 3px 2px 4px 2px; }"
    "QTabBar::tab:selected { background: rgba(128,128,128,72); }"
    "QTabBar::tab:hover:!selected { background: rgba(128,128,128,34); }");

const QString kFilterButtonQss = QStringLiteral(
    "QToolButton { background: transparent; border: none; border-radius: 6px; padding: 3px; }"
    "QToolButton:hover { background: rgba(128,128,128,40); }"
    "QToolButton:checked, QToolButton:pressed { background: rgba(128,128,128,80); }");

// ---------------------------------------------------------------------------------------------
// Main window
// ---------------------------------------------------------------------------------------------

struct MainWindowParts {
    QMainWindow *mw = nullptr;
    QToolBar *bar = nullptr;
    QToolButton *start = nullptr, *app = nullptr, *pref = nullptr, *server = nullptr;
    QCheckBox *vpn = nullptr, *sysProxy = nullptr;
    QPushButton *testBtn = nullptr, *updateBtn = nullptr;
    QToolButton *filterBtn = nullptr;
    QTabWidget *groupTabs = nullptr, *downTabs = nullptr;
    QTableView *proxyTable = nullptr, *connTable = nullptr;
    QLineEdit *logFilter = nullptr, *connFilter = nullptr;
    QList<QLabel *> statusLabels;
};

void applyMainWindowLook(const std::shared_ptr<MainWindowParts> &p, bool sys) {
    // QMainWindow::setUnifiedTitleAndToolBarOnMac(true) makes Qt turn the whole window surface
    // translucent (WA_TranslucentBackground) and ask for a textured window (Qt 6.11.2
    // qcocoawindow.mm: FIXME QTBUG-138829), which on macOS 26/27 leaves every unpainted area of the
    // window see-through. A plain QToolBar band is the safe default; the unified variant is kept
    // behind an environment switch for comparison (PROXOR_MAC_UNIFIED_TOOLBAR=1).
    const bool unified = sys && qEnvironmentVariableIsSet("PROXOR_MAC_UNIFIED_TOOLBAR");
    p->mw->centralWidget()->setAutoFillBackground(unified);
    p->mw->setUnifiedTitleAndToolBarOnMac(unified);

    // The in-window icons (Start/App/Settings/Profiles, the filter funnel) are deliberately left
    // exactly as they were: only the button chrome changes.
    for (auto *b : {p->start, p->app, p->pref, p->server}) {
        b->setStyleSheet(QString());
        b->setAutoRaise(true);
    }
    // Group tabs and Log/Connections tabs.
    for (auto *t : {p->groupTabs, p->downTabs}) {
        t->setDocumentMode(sys);
        t->tabBar()->setDrawBase(!sys);
        t->tabBar()->setStyleSheet(sys ? kTabBarQss : QString());
    }
    if (p->filterBtn) {
        // Chrome only (the icon itself is untouched): flat, with a soft highlight when hovered/checked.
        p->filterBtn->setAutoRaise(true);
        p->filterBtn->setStyleSheet(sys ? kFilterButtonQss : QString());
    }

    // Tables.
    p->proxyTable->horizontalHeader()->setStyleSheet(sys ? kHeaderQss : QString());
    p->proxyTable->setFrameShape(sys ? QFrame::NoFrame : QFrame::StyledPanel);
    p->connTable->setFrameShape(sys ? QFrame::NoFrame : QFrame::StyledPanel);
    p->connTable->verticalHeader()->setVisible(false);
    if (sys) {
        // Monospace is for the log text only.
        p->connTable->setFont(QFont());
        p->connTable->horizontalHeader()->setFont(QFont());
    }

    // Filter fields: system font (the placeholder was monospace), and no keyboard focus at launch.
    for (auto *le : {p->logFilter, p->connFilter}) {
        if (sys) le->setFont(QFont());
        le->setFocusPolicy(sys ? Qt::ClickFocus : Qt::StrongFocus);
        le->setStyleSheet(sys ? kLineEditQss : QString());
    }

    // Status bar: small native font, even margins.
    for (auto *l : p->statusLabels) {
        if (sys) {
            QFont f = QApplication::font();
            f.setPointSizeF(qMax<qreal>(10.0, f.pointSizeF() - 2.0));
            l->setFont(f);
            l->setAlignment(Qt::AlignVCenter | (l->alignment() & Qt::AlignHorizontal_Mask));
        } else {
            l->setFont(QFont());
        }
    }
}

} // namespace

namespace ProxorMac {

void PolishMainWindow(QMainWindow *mw) {
    if (mw == nullptr || mw->centralWidget() == nullptr) return;
    auto *central = mw->centralWidget();
    auto *rootLayout = qobject_cast<QVBoxLayout *>(central->layout());
    auto find = [&](const char *name) { return mw->findChild<QWidget *>(QString::fromLatin1(name)); };

    auto p = std::make_shared<MainWindowParts>();
    p->mw = mw;
    p->start = qobject_cast<QToolButton *>(find("toolButton_toggle_proxy"));
    p->app = qobject_cast<QToolButton *>(find("toolButton_program"));
    p->pref = qobject_cast<QToolButton *>(find("toolButton_preferences"));
    p->server = qobject_cast<QToolButton *>(find("toolButton_server"));
    p->vpn = qobject_cast<QCheckBox *>(find("checkBox_VPN"));
    p->sysProxy = qobject_cast<QCheckBox *>(find("checkBox_SystemProxy"));
    auto *urlTest = qobject_cast<QToolButton *>(find("toolButton_url_test"));
    auto *updateSub = qobject_cast<QToolButton *>(find("toolButton_update_subscription"));
    auto *search = qobject_cast<QLineEdit *>(find("search"));
    p->groupTabs = qobject_cast<QTabWidget *>(find("tabWidget"));
    p->downTabs = qobject_cast<QTabWidget *>(find("down_tab"));
    p->proxyTable = qobject_cast<QTableView *>(find("proxyListTable"));
    p->connTable = qobject_cast<QTableView *>(find("tableWidget_conn"));
    p->logFilter = qobject_cast<QLineEdit *>(find("log_filter"));
    p->connFilter = qobject_cast<QLineEdit *>(find("conn_filter"));
    for (const char *n : {"label_running", "label_inbound", "label_speed"}) {
        if (auto *l = qobject_cast<QLabel *>(find(n))) p->statusLabels << l;
    }
    auto *topRow = central->findChild<QHBoxLayout *>(QStringLiteral("horizontalLayout_2"));
    if (!rootLayout || !topRow || !p->start || !p->app || !p->pref || !p->server || !p->vpn || !p->sysProxy ||
        !urlTest || !updateSub || !search || !p->groupTabs || !p->downTabs || !p->proxyTable || !p->connTable ||
        !p->logFilter || !p->connFilter) {
        return; // the .ui changed under us: leave the window as it is
    }
    p->filterBtn = qobject_cast<QToolButton *>(p->groupTabs->cornerWidget(Qt::TopRightCorner));

    // ---- toolbar -------------------------------------------------------------------------------
    // The search field is only shown on demand (Ctrl+F); keep it above the tabs, out of the row.
    topRow->removeWidget(search);
    rootLayout->insertWidget(0, search);

    p->bar = new QToolBar(mw);
    p->bar->setObjectName(QStringLiteral("macMainToolbar"));
    p->bar->setMovable(false);
    p->bar->setFloatable(false);
    p->bar->setContextMenuPolicy(Qt::PreventContextMenu);
    p->bar->setIconSize(QSize(24, 24));
    p->bar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mw->addToolBar(Qt::TopToolBarArea, p->bar);

    for (auto *b : {p->start, p->app, p->pref, p->server}) {
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setIconSize(QSize(24, 24));
        b->setMinimumSize(0, 0);
        b->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        p->bar->addWidget(b);
    }

    auto *gap1 = new QWidget;
    gap1->setFixedWidth(14);
    p->bar->addWidget(gap1);

    auto *checkBoxes = new QWidget;
    auto *checkBoxLayout = new QVBoxLayout(checkBoxes);
    checkBoxLayout->setContentsMargins(0, 0, 0, 0);
    checkBoxLayout->setSpacing(3);
    for (auto *c : {p->vpn, p->sysProxy}) {
        c->setMinimumHeight(0);
        c->setMaximumHeight(QWIDGETSIZE_MAX);
        c->setStyleSheet(QString());
        c->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        checkBoxLayout->addWidget(c, 0, Qt::AlignLeft | Qt::AlignVCenter);
    }
    p->bar->addWidget(checkBoxes);

    auto *stretch = new QWidget;
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    p->bar->addWidget(stretch);

    // Test Latency / Update Sub: real push buttons with the same width. The original tool buttons
    // stay (hidden) because the rest of MainWindow connects to them; the push buttons forward clicks.
    auto makePush = [](QToolButton *original) {
        auto *btn = new QPushButton(original->text());
        QObject::connect(btn, &QPushButton::clicked, original, &QToolButton::click);
        return btn;
    };
    p->testBtn = makePush(urlTest);
    p->updateBtn = makePush(updateSub);
    urlTest->hide();
    updateSub->hide();
    auto *pushes = new QWidget;
    auto *pushLayout = new QVBoxLayout(pushes);
    pushLayout->setContentsMargins(0, 0, 0, 0);
    pushLayout->setSpacing(4);
    pushLayout->addWidget(p->testBtn);
    pushLayout->addWidget(p->updateBtn);
    p->bar->addWidget(pushes);

    auto *gap2 = new QWidget;
    gap2->setFixedWidth(4);
    p->bar->addWidget(gap2);
    p->bar->setContentsMargins(8, 2, 8, 2);

    // One text size for every control in the bar (the tool button captions were smaller than the
    // check boxes and the push buttons).
    QFont barFont = QApplication::font();
    barFont.setPointSizeF(12);
    for (QWidget *w : {static_cast<QWidget *>(p->start), static_cast<QWidget *>(p->app),
                       static_cast<QWidget *>(p->pref), static_cast<QWidget *>(p->server),
                       static_cast<QWidget *>(p->vpn), static_cast<QWidget *>(p->sysProxy),
                       static_cast<QWidget *>(p->testBtn), static_cast<QWidget *>(p->updateBtn)}) {
        w->setFont(barFont);
    }
    const int pushWidth = qMax(p->testBtn->sizeHint().width(), p->updateBtn->sizeHint().width());
    p->testBtn->setFixedWidth(pushWidth);
    p->updateBtn->setFixedWidth(pushWidth);

    // Not deleted: the generated Ui class still holds pointers to the row's child layouts
    // (verticalLayout_4, verticalLayout_url_sub), which other code reads under the non-System themes.
    rootLayout->removeItem(topRow);

    applyMainWindowLook(p, systemThemeActive());
    QObject::connect(themeManager, &ThemeManager::themeChanged, mw, [p](const QString &) {
        applyMainWindowLook(p, systemThemeActive());
    });
}

} // namespace ProxorMac

