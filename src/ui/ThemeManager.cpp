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

QPalette makeLightPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(245, 245, 245));
    palette.setColor(QPalette::WindowText, QColor(30, 30, 30));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(238, 238, 238));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(30, 30, 30));
    palette.setColor(QPalette::Text, QColor(30, 30, 30));
    palette.setColor(QPalette::Button, QColor(240, 240, 240));
    palette.setColor(QPalette::ButtonText, QColor(30, 30, 30));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(33, 99, 186));
    palette.setColor(QPalette::Highlight, QColor(61, 129, 220));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(135, 135, 135));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(135, 135, 135));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(135, 135, 135));
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
            displayName = QStringLiteral("Windows Native (Vista)");
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

    auto normalizedTheme = NormalizeTheme(theme);

    if (this->current_theme == normalizedTheme && !force) {
        return;
    }

    auto lowerTheme = normalizedTheme.toLower();
    QString baseStyleSheet;

    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setPalette(QPalette());
        qApp->setStyle(this->system_style_name);
    } else if (lowerTheme == "fusion") {
        qApp->setStyleSheet("");
        if (const auto fusionStyle = QStyleFactory::create("Fusion")) {
            qApp->setStyle(fusionStyle);
        }
        qApp->setPalette(QPalette());
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
            qApp->setPalette(QPalette());
            qApp->setStyle(style);
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

    current_theme = normalizedTheme;
    emit themeChanged(normalizedTheme);
}
