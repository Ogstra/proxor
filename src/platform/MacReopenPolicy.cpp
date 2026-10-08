#include "platform/MacReopenPolicy.hpp"

namespace ProxorPlatform {

ReopenAction DecideReopen(bool windowVisible, bool windowMinimized, bool exiting) {
    if (exiting) return ReopenAction::None;
    if (windowVisible && !windowMinimized) return ReopenAction::None;
    return ReopenAction::ShowWindow;
}

bool RepeatActiveDetector::onState(int state) {
    constexpr int kActive = 4;
    const bool repeated = (state == kActive && last_ == kActive);
    last_ = state;
    return repeated;
}

QString ReopenLogLine() {
    return QStringLiteral("Dock: reopen, showing the main window");
}

} // namespace ProxorPlatform
