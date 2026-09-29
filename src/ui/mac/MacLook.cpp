// src/ui/mac/MacLook.cpp — see MacLook.h. Compiled on macOS only (PLATFORM_SOURCES).

#include "MacLook.h"

#include "MacLookCommon.h"

#include <memory>

#include <QApplication>
#include <QCheckBox>
#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QToolButton>

namespace {

using ProxorMac::kLineEditQss;
using ProxorMac::systemThemeActive;

struct MainWindowParts {
    QToolButton *urlTest = nullptr, *updateSub = nullptr;
    QCheckBox *vpn = nullptr, *sysProxy = nullptr;
    QToolButton *filterBtn = nullptr;
    QTabWidget *groupTabs = nullptr, *downTabs = nullptr;
    QTableView *proxyTable = nullptr, *connTable = nullptr;
    QLineEdit *logFilter = nullptr, *connFilter = nullptr;
};

// The window keeps the same layout as on Windows and Linux (the .ui is shared); only the drawing is
// macOS: no style sheets on these widgets, because any QSS makes Qt fall back from the native renderer.
void applyMainWindowLook(const std::shared_ptr<MainWindowParts> &p, bool sys) {
    // Tabs sitting on a framed pane, drawn by the native tab widget (centered on macOS).
    for (auto *t : {p->groupTabs, p->downTabs}) {
        t->setDocumentMode(false);
        t->tabBar()->setDrawBase(true);
        t->tabBar()->setStyleSheet(QString());
    }
    if (p->filterBtn) p->filterBtn->setStyleSheet(QString());

    // Framed tables with a native header; captions centered, as on the other platforms.
    for (auto *table : {p->proxyTable, p->connTable}) {
        table->horizontalHeader()->setStyleSheet(QString());
        table->setFrameShape(QFrame::StyledPanel);
    }
    p->connTable->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);

    // Tun Mode / System Proxy: the .ui gives the first one a Minimum vertical policy, so the layout
    // stretches it taller than the native check box and QMacStyle then draws the indicator above the
    // caption. Pin both to their size hint so indicator and text share one baseline.
    for (auto *c : {p->vpn, p->sysProxy}) {
        c->setSizePolicy(c->sizePolicy().horizontalPolicy(), QSizePolicy::Fixed);
        c->setFixedHeight(c->sizeHint().height());
    }

    // Test Latency / Update Sub: one column, same width.
    const int w = qMax(p->urlTest->sizeHint().width(), p->updateSub->sizeHint().width());
    p->urlTest->setMinimumWidth(w);
    p->updateSub->setMinimumWidth(w);

    // Filter fields: bordered like the other platforms' fields, and no keyboard focus at launch
    // (it drew a focus ring on the empty log filter as soon as the window opened).
    for (auto *le : {p->logFilter, p->connFilter}) {
        le->setFocusPolicy(sys ? Qt::ClickFocus : Qt::StrongFocus);
        le->setStyleSheet(sys ? kLineEditQss : QString());
    }
}

} // namespace

namespace ProxorMac {

void PolishMainWindow(QMainWindow *mw) {
    if (mw == nullptr) return;
    auto find = [&](const char *name) { return mw->findChild<QWidget *>(QString::fromLatin1(name)); };

    auto p = std::make_shared<MainWindowParts>();
    p->urlTest = qobject_cast<QToolButton *>(find("toolButton_url_test"));
    p->updateSub = qobject_cast<QToolButton *>(find("toolButton_update_subscription"));
    p->vpn = qobject_cast<QCheckBox *>(find("checkBox_VPN"));
    p->sysProxy = qobject_cast<QCheckBox *>(find("checkBox_SystemProxy"));
    p->groupTabs = qobject_cast<QTabWidget *>(find("tabWidget"));
    p->downTabs = qobject_cast<QTabWidget *>(find("down_tab"));
    p->proxyTable = qobject_cast<QTableView *>(find("proxyListTable"));
    p->connTable = qobject_cast<QTableView *>(find("tableWidget_conn"));
    p->logFilter = qobject_cast<QLineEdit *>(find("log_filter"));
    p->connFilter = qobject_cast<QLineEdit *>(find("conn_filter"));
    if (!p->urlTest || !p->updateSub || !p->vpn || !p->sysProxy || !p->groupTabs || !p->downTabs || !p->proxyTable || !p->connTable ||
        !p->logFilter || !p->connFilter) {
        return; // the .ui changed under us: leave the window as it is
    }
    p->filterBtn = qobject_cast<QToolButton *>(p->groupTabs->cornerWidget(Qt::TopRightCorner));

    applyMainWindowLook(p, systemThemeActive());
    QObject::connect(themeManager, &ThemeManager::themeChanged, mw, [p](const QString &) {
        applyMainWindowLook(p, systemThemeActive());
    });
}

} // namespace ProxorMac
