#include <QStyle>
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTextStream>
#include <QColor>

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

    auto requestedTheme = theme.trimmed();
    const auto requestedMode = extractThemeMode(&requestedTheme);
    auto normalizedTheme = NormalizeTheme(requestedTheme);

    if (this->current_theme == theme && !force) {
        return;
    }

    auto lowerTheme = normalizedTheme.toLower();
    QString baseStyleSheet;

    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setStyle(this->system_style_name);
        qApp->setPalette(QPalette());
    } else if (lowerTheme == "fusion") {
        qApp->setStyleSheet("");
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
        if (requestedMode == QStringLiteral("light")) {
            qApp->setPalette(makeLightPalette());
        } else if (requestedMode == QStringLiteral("dark")) {
            qApp->setPalette(makeDarkPalette());
        } else {
            qApp->setPalette(QPalette());
        }
    } else if (lowerTheme == "fusionlight") {
        qApp->setStyleSheet("");
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
        qApp->setPalette(makeLightPalette());
    } else if (lowerTheme == "fusiondark") {
        qApp->setStyleSheet("");
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
        qApp->setPalette(makeDarkPalette());
    } else if (lowerTheme == "fusionarcdark") {
        qApp->setStyleSheet("");
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
        qApp->setPalette(makeArcDarkPalette());
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
            qApp->setStyle(style);
            if (requestedMode == QStringLiteral("light")) {
                qApp->setPalette(makeLightPalette());
            } else if (requestedMode == QStringLiteral("dark")) {
                qApp->setPalette(makeDarkPalette());
            } else {
                qApp->setPalette(QPalette());
            }
        } else {
            qApp->setStyleSheet("");
            qApp->setPalette(QPalette());
            qApp->setStyle(this->system_style_name);
            normalizedTheme = "System";
        }
    }
    qApp->setStyleSheet(baseStyleSheet);

    // Force unpolish/repolish on every widget so QSS subcontrol overrides
    // (e.g. arrow suppression) take effect even when the style object itself
    // didn't change (e.g. System theme at startup).
    for (auto *w : qApp->allWidgets()) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }

    current_theme = theme;
    emit themeChanged(current_theme);
}
