#pragma once

// Windows only (listed in cmake/windows/windows.cmake): a small badge on the taskbar button while a profile runs.
// A taskbar button pinned to the taskbar keeps the shortcut's icon and ignores the window icon, so the status
// icon never showed there; an overlay icon works for pinned and unpinned buttons.

class QImage;
class QWidget;

namespace ProxorWin {

// Sets the wanted badge (a null image removes it) and applies it; a taskbar button that does not exist yet
// (window hidden, Explorer restarting) is retried a few times. ReapplyTaskbarOverlay repeats the last badge,
// e.g. when the window is shown.
void SetTaskbarOverlay(QWidget *window, const QImage &badge);
void ReapplyTaskbarOverlay(QWidget *window);

} // namespace ProxorWin
