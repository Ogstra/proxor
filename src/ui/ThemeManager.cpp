#include <QStyle>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QWidget>
#include <QFile>
#include <QLayout>
#include <QPalette>
#include <QStyleFactory>
#include <QStyleHints>
#include <QTextStream>
#include <QTimer>
#include <QColor>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

#include "ThemeManager.hpp"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

namespace {
QString loadStyleSheet(const QString &resourcePath) {
    return ReadFileText(resourcePath);
}

QStyle *createStyleOrNull(const QString &styleName) {
    return QStyleFactory::create(styleName);
}

void copyActiveToInactive(QPalette *palette) {
    const auto roles = {
        QPalette::Window,
        QPalette::WindowText,
        QPalette::Base,
        QPalette::AlternateBase,
        QPalette::ToolTipBase,
        QPalette::ToolTipText,
        QPalette::Text,
        QPalette::Button,
        QPalette::ButtonText,
        QPalette::BrightText,
        QPalette::Link,
        QPalette::Highlight,
        QPalette::HighlightedText,
        QPalette::Light,
        QPalette::Midlight,
        QPalette::Mid,
        QPalette::Dark,
        QPalette::Shadow,
        QPalette::PlaceholderText
    };
    for (const auto role : roles) {
        palette->setColor(QPalette::Inactive, role, palette->color(QPalette::Active, role));
    }
}

QString extractThemeMode(QString *themeName) {
    const int separator = themeName->lastIndexOf('|');
    if (separator < 0) return {};
    const auto mode = themeName->mid(separator + 1).trimmed().toLower();
    *themeName = themeName->left(separator).trimmed();
    if (mode == QStringLiteral("light") || mode == QStringLiteral("dark") || mode == QStringLiteral("system")) {
        return mode;
    }
    return {};
}

QPalette makeLightPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(241, 243, 246));
    palette.setColor(QPalette::WindowText, QColor(20, 24, 29));
    palette.setColor(QPalette::Base, QColor(229, 233, 238));
    palette.setColor(QPalette::AlternateBase, QColor(221, 226, 232));
    palette.setColor(QPalette::ToolTipBase, QColor(245, 247, 250));
    palette.setColor(QPalette::ToolTipText, QColor(24, 28, 33));
    palette.setColor(QPalette::Text, QColor(24, 28, 33));
    palette.setColor(QPalette::Button, QColor(223, 228, 234));
    palette.setColor(QPalette::ButtonText, QColor(20, 24, 29));
    palette.setColor(QPalette::Light, QColor(250, 251, 252));
    palette.setColor(QPalette::Midlight, QColor(233, 237, 242));
    palette.setColor(QPalette::PlaceholderText, QColor(106, 115, 127));
    palette.setColor(QPalette::Mid, QColor(164, 173, 185));
    palette.setColor(QPalette::Dark, QColor(128, 137, 149));
    palette.setColor(QPalette::Shadow, QColor(84, 93, 105));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(18, 87, 176));
    palette.setColor(QPalette::Highlight, QColor(25, 105, 205));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Base, QColor(235, 238, 242));
    palette.setColor(QPalette::Disabled, QPalette::Button, QColor(231, 234, 238));
    palette.setColor(QPalette::Disabled, QPalette::Window, QColor(237, 240, 243));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(123, 132, 143));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(123, 132, 143));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(123, 132, 143));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(160, 186, 221));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(243, 246, 249));
    copyActiveToInactive(&palette);
    return palette;
}

QPalette makeDarkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(45, 45, 48));
    palette.setColor(QPalette::WindowText, QColor(230, 230, 230));
    palette.setColor(QPalette::Base, QColor(30, 30, 30));
    palette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
    palette.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
    palette.setColor(QPalette::Text, QColor(230, 230, 230));
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, QColor(230, 230, 230));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(78, 148, 255));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
    copyActiveToInactive(&palette);
    return palette;
}

QPalette makeArcDarkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(56, 60, 74));
    palette.setColor(QPalette::WindowText, QColor(211, 218, 227));
    palette.setColor(QPalette::Base, QColor(47, 52, 63));
    palette.setColor(QPalette::AlternateBase, QColor(64, 69, 82));
    palette.setColor(QPalette::ToolTipBase, QColor(47, 52, 63));
    palette.setColor(QPalette::ToolTipText, QColor(211, 218, 227));
    palette.setColor(QPalette::Text, QColor(211, 218, 227));
    palette.setColor(QPalette::Button, QColor(64, 69, 82));
    palette.setColor(QPalette::ButtonText, QColor(211, 218, 227));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(82, 148, 226));
    palette.setColor(QPalette::Highlight, QColor(82, 148, 226));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(131, 140, 155));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(131, 140, 155));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(131, 140, 155));
    copyActiveToInactive(&palette);
    return palette;
}


bool systemPrefersDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return QPalette().color(QPalette::Window).lightness() < 128;
#endif
}

QPalette paletteForMode(const QString &requestedMode) {
    if (requestedMode == QStringLiteral("light")) return makeLightPalette();
    if (requestedMode == QStringLiteral("dark")) return makeDarkPalette();
    return systemPrefersDark() ? makeDarkPalette() : makeLightPalette();
}

QPalette nativePaletteForMode(const QString &requestedMode) {
    if (requestedMode == QStringLiteral("light")) return makeLightPalette();
    if (requestedMode == QStringLiteral("dark")) return makeDarkPalette();
    return QPalette();
}

bool resolvedIsDark(const QString &lowerTheme, const QString &requestedMode) {
    if (lowerTheme == QStringLiteral("qdarkstyle") ||
        lowerTheme == QStringLiteral("fusionarcdark") ||
        lowerTheme == QStringLiteral("fusiondark")) {
        return true;
    }
    if (lowerTheme == QStringLiteral("fusionlight")) return false;
    if (requestedMode == QStringLiteral("dark")) return true;
    if (requestedMode == QStringLiteral("light")) return false;
    return systemPrefersDark();
}

bool usesFusionMetrics(const QString &lowerTheme) {
    return lowerTheme == QStringLiteral("fusion") ||
           lowerTheme == QStringLiteral("fusionlight") ||
           lowerTheme == QStringLiteral("fusiondark") ||
           lowerTheme == QStringLiteral("fusionarcdark");
}

constexpr auto kOriginalComboMinHeightProperty = "proxorOriginalComboMinHeight";

int comboBoxMinimumHeight(const QComboBox *combo) {
    const int contentHeight = combo->fontMetrics().height() + 10;
    return qMax(combo->sizeHint().height(), contentHeight);
}

void applyComboBoxMetrics(QComboBox *combo, bool fusionMetrics) {
    if (combo == nullptr) return;

    if (!combo->property(kOriginalComboMinHeightProperty).isValid()) {
        combo->setProperty(kOriginalComboMinHeightProperty, combo->minimumHeight());
    }

    const int originalMinHeight = combo->property(kOriginalComboMinHeightProperty).toInt();
    if (fusionMetrics) {
        combo->setMinimumHeight(qMax(originalMinHeight, comboBoxMinimumHeight(combo)));
    } else {
        combo->setMinimumHeight(originalMinHeight);
    }
    combo->updateGeometry();
}

void applyComboBoxMetrics(QWidget *w, bool fusionMetrics) {
    if (w == nullptr) return;
    if (auto *combo = qobject_cast<QComboBox *>(w)) {
        applyComboBoxMetrics(combo, fusionMetrics);
    }
    if (w->isWindow()) {
        for (auto *combo : w->findChildren<QComboBox *>()) {
            applyComboBoxMetrics(combo, fusionMetrics);
        }
    }
}

void refreshWidgetGeometry(QWidget *w) {
    if (w == nullptr) return;
    w->updateGeometry();
    if (auto *layout = w->layout()) {
        layout->invalidate();
        layout->activate();
    }
}

void refreshAllWidgetGeometry() {
    for (auto *w : qApp->allWidgets()) {
        refreshWidgetGeometry(w);
    }
    for (auto *w : qApp->topLevelWidgets()) {
        refreshWidgetGeometry(w);
    }
}

#ifdef Q_OS_WIN
constexpr DWORD kDwmColorDefault = 0xFFFFFFFF;
constexpr DWORD kDwmCaptionColorAttribute = 35; // DWMWA_CAPTION_COLOR
constexpr DWORD kDwmTextColorAttribute = 36;    // DWMWA_TEXT_COLOR

void setDwmAttribute(HWND hwnd, DWORD attribute, const void *value, DWORD size) {
    DwmSetWindowAttribute(hwnd, attribute, value, size);
}

void applyDarkTitleBarToWidget(QWidget *w, bool dark) {
    if (w == nullptr || !w->isWindow()) return;
    const BOOL value = dark ? TRUE : FALSE;
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &value, sizeof(value));

    const DWORD captionColor = dark ? RGB(32, 32, 32) : kDwmColorDefault;
    const DWORD textColor = dark ? RGB(255, 255, 255) : kDwmColorDefault;
    setDwmAttribute(hwnd, kDwmCaptionColorAttribute, &captionColor, sizeof(captionColor));
    setDwmAttribute(hwnd, kDwmTextColorAttribute, &textColor, sizeof(textColor));

    // Force an immediate non-client redraw; DWM otherwise can keep the old
    // active/inactive title bar colors until the next frame change.
    if (w->isVisible()) {
        RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

void applyDarkTitleBar(bool dark) {
    for (QWidget *w : qApp->topLevelWidgets()) {
        applyDarkTitleBarToWidget(w, dark);
    }
}
#endif

void reloadWidgetStyleState(bool repolish, bool fusionMetrics) {
    for (auto *w : qApp->allWidgets()) {
        // A global QSS theme can leave resolved palettes on existing widgets.
        // Clear local palette state so the widget resolves colors like it would
        // after being constructed under the newly selected application theme.
        w->setPalette(QPalette());
        if (repolish) {
            w->style()->unpolish(w);
            w->style()->polish(w);
        }
        applyComboBoxMetrics(w, fusionMetrics);
        refreshWidgetGeometry(w);
        w->update();
    }
    refreshAllWidgetGeometry();
    QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QTimer::singleShot(0, qApp, []() {
        refreshAllWidgetGeometry();
        QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    });
}

}

QList<ThemeManager::ThemeOption> ThemeManager::AvailableThemes() const {
    QList<ThemeOption> themes;
    themes.append({QStringLiteral("System"), QStringLiteral("System")});

    const auto sysName = system_style_name.toLower();
    for (const auto &key : QStyleFactory::keys()) {
        if (key.toLower() == sysName)
            continue;
        if (key.toLower() == QStringLiteral("fusion"))
            continue;  // Fusion is used as QDarkStyle base, skip to avoid confusion
        QString displayName = key;
        if (key.compare(QStringLiteral("Windows"), Qt::CaseInsensitive) == 0) {
            displayName = QStringLiteral("Windows Classic");
        } else if (key.compare(QStringLiteral("WindowsVista"), Qt::CaseInsensitive) == 0) {
            continue;
        }
        themes.append({key, displayName});
    }

    themes.append({QStringLiteral("Fusion"), QStringLiteral("Fusion")});
    themes.append({QStringLiteral("FusionArcDark"), QStringLiteral("Arc Dark")});
    themes.append({QStringLiteral("QDarkStyle"), QStringLiteral("QDarkStyle")});

    return themes;
}

QString ThemeManager::NormalizeTheme(const QString &theme) const {
    auto normalizedTheme = theme.trimmed();
    extractThemeMode(&normalizedTheme);
    if (normalizedTheme.isEmpty()) {
        return QStringLiteral("System");
    }

    bool legacyNumericTheme = false;
    normalizedTheme.toInt(&legacyNumericTheme);
    if (legacyNumericTheme) {
        return QStringLiteral("System");
    }

    const auto lowerTheme = normalizedTheme.toLower();
    if (lowerTheme == QStringLiteral("fusion")) {
        return QStringLiteral("Fusion");
    }
    if (lowerTheme == QStringLiteral("fusionlight") || lowerTheme == QStringLiteral("light")) {
        return QStringLiteral("FusionLight");
    }
    if (lowerTheme == QStringLiteral("fusiondark")) {
        return QStringLiteral("FusionDark");
    }
    if (lowerTheme == QStringLiteral("fusionarcdark") || lowerTheme == QStringLiteral("arcdark") || lowerTheme == QStringLiteral("arc-dark")) {
        return QStringLiteral("FusionArcDark");
    }
    if (lowerTheme == QStringLiteral("qdarkstyle") || lowerTheme == QStringLiteral("dark")) {
        return QStringLiteral("QDarkStyle");
    }
    if (lowerTheme == QStringLiteral("system")) {
        return QStringLiteral("System");
    }

    // Check if it is a valid QStyleFactory key (case-insensitive match)
    for (const auto &key : QStyleFactory::keys()) {
        if (key.toLower() == lowerTheme) {
            return key;  // Return the canonical casing from QStyleFactory
        }
    }

    return QStringLiteral("System");
}

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }
    applying = true;

    auto requestedTheme = theme.trimmed();
    const auto requestedMode = extractThemeMode(&requestedTheme);
    auto normalizedTheme = NormalizeTheme(requestedTheme);

    if (this->current_theme == theme && !force) {
        applying = false;
        return;
    }

    auto lowerTheme = normalizedTheme.toLower();
    QString appliedTheme = theme;
    QString baseStyleSheet;

    // Palette must be set BEFORE the style: native styles resolve their
    // light/dark rendering at polish time (during setStyle), and the explicit
    // setPalette keeps setStyle from resetting it to the style's standard palette.
    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setPalette(QPalette());
        qApp->setStyle(this->system_style_name);
    } else if (lowerTheme == "fusion") {
        qApp->setStyleSheet("");
        qApp->setPalette(paletteForMode(requestedMode));
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
    } else if (lowerTheme == "fusionlight") {
        qApp->setStyleSheet("");
        qApp->setPalette(makeLightPalette());
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
    } else if (lowerTheme == "fusiondark") {
        qApp->setStyleSheet("");
        qApp->setPalette(makeDarkPalette());
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
    } else if (lowerTheme == "fusionarcdark") {
        qApp->setStyleSheet("");
        qApp->setPalette(makeArcDarkPalette());
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
    } else if (lowerTheme == "qdarkstyle") {
        qApp->setStyleSheet("");
        qApp->setPalette(QPalette());
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
        baseStyleSheet = loadStyleSheet(":/qdarkstyle/dark/darkstyle.qss");
        baseStyleSheet.append(loadStyleSheet(":/proxor/qdarkstyle_overrides.qss"));
    } else {
        const auto style = QStyleFactory::create(normalizedTheme);
        if (style != nullptr) {
            qApp->setStyleSheet("");
            qApp->setPalette(nativePaletteForMode(requestedMode));
            qApp->setStyle(style);
        } else {
            qApp->setStyleSheet("");
            qApp->setPalette(QPalette());
            qApp->setStyle(this->system_style_name);
            normalizedTheme = "System";
            lowerTheme = normalizedTheme.toLower();
            appliedTheme = normalizedTheme;
        }
    }
    qApp->setStyleSheet(baseStyleSheet);

    current_theme = appliedTheme;

    if (!event_filter_installed) {
        qApp->installEventFilter(this);
        event_filter_installed = true;
    }

#ifdef Q_OS_WIN
    title_bar_dark = resolvedIsDark(lowerTheme, requestedMode);
    applyDarkTitleBar(title_bar_dark);
#endif

    // Native platform styles must not be unpolished/polished (it corrupts their
    // UxTheme state). Fusion/QSS themes can be fully repolished. In both cases
    // clear widget palettes so existing tables/lists don't keep colors resolved
    // from the previous global stylesheet.
    const bool customStyle = lowerTheme == "fusion" || lowerTheme == "fusionlight" ||
                             lowerTheme == "fusiondark" || lowerTheme == "fusionarcdark" ||
                             lowerTheme == "qdarkstyle";
    reloadWidgetStyleState(customStyle, usesFusionMetrics(lowerTheme));

    applying = false;
    emit themeChanged(current_theme);
}

void ThemeManager::ReapplyTitleBar() {
#ifdef Q_OS_WIN
    auto normalizedTheme = current_theme.trimmed();
    extractThemeMode(&normalizedTheme);
    const auto lowerTheme = NormalizeTheme(normalizedTheme).toLower();

    QString modeTemp = current_theme.trimmed();
    const auto requestedMode = extractThemeMode(&modeTemp);

    title_bar_dark = resolvedIsDark(lowerTheme, requestedMode);
    applyDarkTitleBar(title_bar_dark);
#endif
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::Show) {
        auto normalizedTheme = current_theme.trimmed();
        extractThemeMode(&normalizedTheme);
        const auto lowerTheme = NormalizeTheme(normalizedTheme).toLower();
        if (auto *w = qobject_cast<QWidget *>(watched)) {
            applyComboBoxMetrics(w, usesFusionMetrics(lowerTheme));
            refreshWidgetGeometry(w);
        }
    }

#ifdef Q_OS_WIN
    // Dialogs and other sub-windows are created after the theme was applied, so
    // they miss the title bar attribute. Apply it as each one becomes visible.
    if (event->type() == QEvent::Show ||
        event->type() == QEvent::WindowActivate ||
        event->type() == QEvent::WindowDeactivate ||
        event->type() == QEvent::ActivationChange) {
        if (auto *w = qobject_cast<QWidget *>(watched); w != nullptr && w->isWindow()) {
            applyDarkTitleBarToWidget(w, title_bar_dark);
        }
    }
#endif
    return QObject::eventFilter(watched, event);
}
