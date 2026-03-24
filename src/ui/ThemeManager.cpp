#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTextStream>

#include "ThemeManager.hpp"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    auto normalizedTheme = theme.trimmed();
    if (normalizedTheme.isEmpty()) {
        normalizedTheme = "System";
    }

    bool legacyNumericTheme = false;
    normalizedTheme.toInt(&legacyNumericTheme);
    if (legacyNumericTheme) {
        normalizedTheme = "System";
    }

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
        QFile file(":/qdarkstyle/dark/darkstyle.qss");
        if (file.open(QFile::ReadOnly | QFile::Text)) {
            QTextStream stream(&file);
            baseStyleSheet = stream.readAll();
        }
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

    auto proxorCss = ReadFileText(":/proxor/proxor.css");
    if (!baseStyleSheet.isEmpty() && !proxorCss.isEmpty()) {
        baseStyleSheet.append("\n");
    }
    baseStyleSheet.append(proxorCss);
    qApp->setStyleSheet(baseStyleSheet);

    current_theme = normalizedTheme;
    emit themeChanged(normalizedTheme);
}
