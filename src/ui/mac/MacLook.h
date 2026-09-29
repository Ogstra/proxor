// src/ui/mac/MacLook.h — runtime macOS look-and-feel adjustments. Plain Qt (no Objective-C).
//
// The .ui files are shared with Windows and Linux, so none of this edits their geometry: the
// main window and every Proxor dialog are adjusted after setupUi, by object name, and only on
// macOS (this file is compiled through PLATFORM_SOURCES on APPLE only). Include only under
// #ifdef Q_OS_MACOS.
#pragma once

class QMainWindow;

namespace ProxorMac {
// Keeps the main window's layout identical to Windows/Linux and only adjusts how it is drawn on
// macOS: native framed tab panes and tables, equal-width Test Latency / Update Sub, bordered filter
// fields without launch focus. Call once at the end of the MainWindow constructor; re-evaluates
// itself whenever the theme changes.
void PolishMainWindow(QMainWindow *mainWindow);
}
