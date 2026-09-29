// src/ui/mac/MacPlatform.mm — macOS-only implementation. See MacPlatform.h.
//
// Root cause of the tray crash (quick 260928-ncf/tray-crash.log), confirmed against
// https://raw.githubusercontent.com/qt/qtbase/v6.11.2/src/plugins/platforms/cocoa/qcocoasystemtrayicon.mm:
// QCocoaSystemTrayIcon::emitActivated() (line 245) reads `NSApp.currentEvent.clickCount`
// (line 247/251). It is invoked both as the NSStatusItem button's action
// (-statusItemClicked, wired in QCocoaSystemTrayIcon's init) and, whenever
// QSystemTrayIcon::setContextMenu() assigns a menu, from
// -statusItemMenuBeganTracking: (registered in ::updateMenu() for
// NSMenuDidBeginTrackingNotification on the tray NSMenu). On macOS 27 a status-item
// menu opened through the FrontBoard scene action makes NSApp.currentEvent a
// SysDefined event, and -[NSEvent clickCount] throws NSInternalInconsistencyException
// for any non-mouse event type. An NSStatusItem owned directly by Proxor (this file),
// whose menu AppKit pops up itself, never goes through QCocoaSystemTrayIcon and never
// reaches that read.

#import <AppKit/AppKit.h>

#include "MacPlatform.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QIcon>
#include <QWidget>
#include <QImage>
#include <QMenu>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QTimer>

namespace ProxorMac {

struct StatusItem::Impl {
    NSStatusItem *item = nil;
    QIcon icon;
    bool colored = true;

    // Template variant of a colored icon: only the alpha channel matters to AppKit, so the
    // result is black. Saturated pixels (the colored arcs) stay fully opaque; unsaturated ones
    // (the white arc of the idle icon) keep 38% of their alpha as a "ghost", which is what
    // separates the idle glyph from the running one (both arcs solid).
    static QImage monochrome(const QImage &src) {
        QImage img = src.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < img.height(); ++y) {
            auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                const QRgb px = line[x];
                const int a = qAlpha(px);
                if (a == 0) continue;
                const int mx = qMax(qRed(px), qMax(qGreen(px), qBlue(px)));
                const int mn = qMin(qRed(px), qMin(qGreen(px), qBlue(px)));
                const int outA = (mx - mn) > 60 ? a : (a * 38) / 100;
                line[x] = qRgba(0, 0, 0, outA);
            }
        }
        return img;
    }

    void render() {
        if (icon.isNull()) return;
        const qreal thickness = NSStatusBar.systemStatusBar.thickness;
        const int h = qMax(1, static_cast<int>(thickness - 4));
        qreal dpr = qApp ? qApp->devicePixelRatio() : 1.0;
        QPixmap pm = icon.pixmap(QSize(h, h), dpr);
        if (pm.isNull()) return;

        QImage image = pm.toImage();
        if (!colored) image = monochrome(image);
        CGImageRef cg = image.toCGImage();
        if (!cg) return;

        const qreal pmDpr = pm.devicePixelRatio() > 0 ? pm.devicePixelRatio() : 1.0;
        NSSize size = NSMakeSize(pm.width() / pmDpr, pm.height() / pmDpr);
        NSImage *nsImage = [[NSImage alloc] initWithCGImage:cg size:size];
        CGImageRelease(cg);

        // `template` is a C++ keyword, so the AppKit property is set with bracket syntax.
        // Colored: the color is Proxor's running/idle indicator, so it must not be tinted.
        [nsImage setTemplate:colored ? NO : YES];
        item.button.image = nsImage;
        item.button.imageScaling = NSImageScaleProportionallyDown;
        [nsImage release];
    }
};

StatusItem::StatusItem() : d(new Impl) {
    d->item = [[NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength] retain];
}

StatusItem::~StatusItem() {
    if (d->item) {
        [NSStatusBar.systemStatusBar removeStatusItem:d->item];
        [d->item release];
        d->item = nil;
    }
    delete d;
}

void StatusItem::setMenu(QMenu *menu) {
    // Do not set a button action or target: with a menu assigned, AppKit pops it up
    // itself and no Qt code reads NSApp.currentEvent for this status item.
    d->item.menu = menu ? menu->toNSMenu() : nil;
}

void StatusItem::setIcon(const QIcon &icon) {
    d->icon = icon;
    d->render();
}

void StatusItem::setColored(bool colored) {
    if (d->colored == colored) return;
    d->colored = colored;
    d->render();
}

void StatusItem::setToolTip(const QString &text) {
    d->item.button.toolTip = text.toNSString();
}

void StatusItem::setVisible(bool visible) {
    d->item.visible = visible;
}

bool StatusItem::isVisible() const {
    return d->item.visible;
}

namespace {
bool g_allowQuit = false;

class QuitInterceptor : public QObject {
public:
    QuitInterceptor(QObject *owner, std::function<void()> onQuit)
        : QObject(owner), m_onQuit(std::move(onQuit)) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (!g_allowQuit && watched == qApp && event->type() == QEvent::Quit && event->spontaneous()) {
            // Both are required: Qt's applicationShouldTerminate reads
            // event.isAccepted() to decide NSTerminateNow vs NSTerminateCancel.
            event->ignore();
            QTimer::singleShot(0, this, m_onQuit);
            return true;
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> m_onQuit;
};
}

void InstallQuitInterceptor(QObject *owner, std::function<void()> onQuit) {
    auto *interceptor = new QuitInterceptor(owner, std::move(onQuit));
    qApp->installEventFilter(interceptor);
}

void AllowQuit() {
    g_allowQuit = true;
}

void PopupMenuAt(QMenu *menu, QWidget *anchor, const QPoint &posInAnchor) {
    if (!menu || !anchor || !anchor->window()) return;
    NSMenu *nsMenu = menu->toNSMenu();
    NSView *view = reinterpret_cast<NSView *>(anchor->window()->winId());
    if (!nsMenu || !view) return;
    // QNSView is flipped, so view coordinates run top-down like Qt's.
    const QPoint p = anchor->mapTo(anchor->window(), posInAnchor);
    [nsMenu popUpMenuPositioningItem:nil atLocation:NSMakePoint(p.x(), p.y()) inView:view];
}

void PopupMenu(QMenu *menu, QWidget *anchor) {
    if (!anchor) return;
    PopupMenuAt(menu, anchor, QPoint(0, anchor->height() + 2));
}

}
