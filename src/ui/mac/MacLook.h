// src/ui/mac/MacLook.h — runtime macOS look-and-feel adjustments. Plain Qt (no Objective-C).
//
// The .ui files are shared with Windows and Linux, so none of this edits their geometry: the
// main window and every Proxor dialog are adjusted after setupUi, by object name, and only on
// macOS (this file is compiled through PLATFORM_SOURCES on APPLE only). Include only under
// #ifdef Q_OS_MACOS.
#pragma once

class QMainWindow;

namespace ProxorMac {
// Turns the top button row into a real QToolBar (text under icon, native push buttons on the right),
// and applies native-looking tabs, headers, fonts and status bar under the System theme. The icons
// inside the window are left untouched. Call once at the end of the MainWindow constructor;
// re-evaluates itself whenever the theme changes.
void PolishMainWindow(QMainWindow *mainWindow);
}
