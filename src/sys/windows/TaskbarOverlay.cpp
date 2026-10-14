#include "sys/windows/TaskbarOverlay.h"

#include <QImage>
#include <QPainter>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <windows.h>
#include <shobjidl.h>

#include <cstring>

namespace {

bool g_active = false;

HICON makeBadgeIcon() {
    QImage img(32, 32, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(Qt::white, 2.5));
        p.setBrush(QColor(232, 48, 48));
        p.drawEllipse(QRectF(5, 5, 22, 22));
    }
    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(bi);
    bi.bV5Width = img.width();
    bi.bV5Height = -img.height(); // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    void *bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!color || !bits) return nullptr;
    std::memcpy(bits, img.constBits(), size_t(img.sizeInBytes()));
    HBITMAP mask = CreateBitmap(img.width(), img.height(), 1, 1, nullptr);
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmMask = mask;
    ii.hbmColor = color;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

bool applyOverlay(HWND hwnd, bool active) {
    if (!hwnd) return false;
    ITaskbarList3 *taskbar = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))) || !taskbar) return false;
    bool ok = false;
    if (SUCCEEDED(taskbar->HrInit())) {
        HICON icon = active ? makeBadgeIcon() : nullptr;
        ok = SUCCEEDED(taskbar->SetOverlayIcon(hwnd, icon, active ? L"Running" : L""));
        if (icon) DestroyIcon(icon);
    }
    taskbar->Release();
    return ok;
}

void tryApply(QPointer<QWidget> window, int attemptsLeft) {
    if (!window) return;
    if (applyOverlay(reinterpret_cast<HWND>(window->winId()), g_active)) return;
    if (attemptsLeft <= 0) return;
    QTimer::singleShot(1000, window.data(), [window, attemptsLeft] { tryApply(window, attemptsLeft - 1); });
}

} // namespace

namespace ProxorWin {

void SetTaskbarActiveOverlay(QWidget *window, bool active) {
    g_active = active;
    tryApply(QPointer<QWidget>(window), 5);
}

void ReapplyTaskbarOverlay(QWidget *window) {
    tryApply(QPointer<QWidget>(window), 5);
}

} // namespace ProxorWin
