#include <QStyle>
#include <QApplication>
#include <QWidget>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTextStream>

#include "ThemeManager.hpp"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

namespace {
QString loadStyleSheet(const QString &resourcePath) {
    return ReadFileText(resourcePath);
}
}

QList<ThemeManager::ThemeOption> ThemeManager::AvailableThemes() const {
    QList<ThemeOption> themes;
    themes.append({QStringLiteral("System"), QStringLiteral("System")});
    themes.append({QStringLiteral("QDarkStyle"), QStringLiteral("QDarkStyle")});

    const auto sysName = system_style_name.toLower();
    for (const auto &key : QStyleFactory::keys()) {
        if (key.toLower() == sysName)
            continue;
        if (key.toLower() == QStringLiteral("fusion"))
            continue;  // Fusion is used as QDarkStyle base, skip to avoid confusion
        themes.append({key, key});
    }

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

    auto proxorCss = loadStyleSheet(":/proxor/proxor.css");
    if (!baseStyleSheet.isEmpty() && !proxorCss.isEmpty()) {
        baseStyleSheet.append("\n");
    }
    baseStyleSheet.append(proxorCss);
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
