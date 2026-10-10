// src/ui/mac/MacLookCommon.h — style sheets and helpers shared by MacLook.cpp and MacDialogs.cpp.
// Compiled on macOS only.
#pragma once

#include <QString>

#include "main/ProxorGui.hpp"
#include "ui/ThemeManager.hpp"

namespace ProxorMac {

// True while the System theme is selected: the native look applies only there; the Fusion/QSS
// themes keep their own styling.
// NOTE: ThemeManager::NormalizeTheme maps "System" to "Fusion" on macOS while the native System theme
// is hidden, so this is currently always false here and the System branches below (kLineEditQss,
// MacLook.cpp, MacDialogs.cpp) are intentionally dormant until the native theme is enabled again.
// Do not enable them without a visual check on a Mac.
inline bool systemThemeActive() {
    return themeManager != nullptr &&
           themeManager->NormalizeTheme(ProxorGui::dataStore->theme) == QStringLiteral("System");
}

// Translucent grays read correctly on both the light and the dark appearance without a custom palette.
inline const QString kHeaderQss = QStringLiteral(
    "QHeaderView::section { background: transparent; border: none;"
    "  border-bottom: 1px solid rgba(128,128,128,70); padding: 3px 8px; }");

inline const QString kLineEditQss = QStringLiteral(
    "QLineEdit { background: palette(base); border: 1px solid rgba(128,128,128,110);"
    "  border-radius: 5px; padding: 2px 6px; selection-background-color: palette(highlight); }"
    "QLineEdit:focus { border: 1px solid palette(highlight); }"
    "QLineEdit:disabled { border-color: rgba(128,128,128,50); }");

} // namespace ProxorMac
