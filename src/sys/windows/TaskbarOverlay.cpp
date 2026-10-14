#include "sys/windows/TaskbarOverlay.h"

#include <QImage>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <windows.h>
#include <shobjidl.h>

#include <cstring>

namespace {

QImage g_badge; // 32x32 ARGB, null: no badge

HICON makeBadgeIcon(const QImage &badge) {
    const QImage img = badge.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation).convertToFormat(QImage::Format_ARGB32);
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

bool applyOverlay(HWND hwnd, const QImage &badge) {
    if (!hwnd) return false;
    ITaskbarList3 *taskbar = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&taskbar))) || !taskbar) return false;
    bool ok = false;
    if (SUCCEEDED(taskbar->HrInit())) {
        HICON icon = badge.isNull() ? nullptr : makeBadgeIcon(badge);
        ok = SUCCEEDED(taskbar->SetOverlayIcon(hwnd, icon, badge.isNull() ? L"" : L"Running"));
        if (icon) DestroyIcon(icon);
    }
    taskbar->Release();
    return ok;
}

void tryApply(QPointer<QWidget> window, int attemptsLeft) {
    if (!window) return;
    if (applyOverlay(reinterpret_cast<HWND>(window->winId()), g_badge)) return;
    if (attemptsLeft <= 0) return;
    QTimer::singleShot(1000, window.data(), [window, attemptsLeft] { tryApply(window, attemptsLeft - 1); });
}

} // namespace

namespace ProxorWin {

void SetTaskbarOverlay(QWidget *window, const QImage &badge) {
    g_badge = badge;
    tryApply(QPointer<QWidget>(window), 5);
}

void ReapplyTaskbarOverlay(QWidget *window) {
    tryApply(QPointer<QWidget>(window), 5);
}

} // namespace ProxorWin
