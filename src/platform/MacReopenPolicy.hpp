// src/platform/MacReopenPolicy.hpp — when a macOS "reopen" (Dock icon click) must show the main window.
// Qt Core only, pure; used by the macOS reopen hook and tested on every runner.
#pragma once
#include <QString>

namespace ProxorPlatform {

enum class ReopenAction { None, ShowWindow };

// exiting: Proxor is quitting. A visible, non-minimized window needs nothing from Proxor (macOS
// already activates the app); hidden or minimized -> show it.
ReopenAction DecideReopen(bool windowVisible, bool windowMinimized, bool exiting);

// Qt-signal route only: feed every applicationStateChanged value (Qt::ApplicationState as int);
// returns true when Active (4) follows Active, which is Qt's forced state change on a Dock reopen.
// The first Active after launch never fires.
class RepeatActiveDetector {
public:
    bool onState(int state);

private:
    int last_ = -1;
};

QString ReopenLogLine();

} // namespace ProxorPlatform
