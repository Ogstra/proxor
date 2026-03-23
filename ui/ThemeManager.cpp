#include <QStyle>
#include <QApplication>
#include <QStyleFactory>

#include "ThemeManager.hpp"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

void ThemeManager::ApplyTheme(const QString &theme) {
    auto internal = [=] {
        if (this->system_style_name.isEmpty()) {
            this->system_style_name = qApp->style()->objectName();
        }
        if (this->current_theme == theme) {
            return;
        }

        bool ok;
        auto themeId = theme.toInt(&ok);

        if (ok) {
            // Only themeId == 0 (System) is valid after feiyangqingyun removal
            auto system_style = QStyleFactory::create(this->system_style_name);
            qApp->setPalette(QPalette());
            qApp->setStyle(system_style);
            qApp->setStyleSheet("");
        } else {
            // QStyleFactory
            const auto &_style = QStyleFactory::create(theme);
            if (_style != nullptr) {
                qApp->setPalette(_style->standardPalette());
                qApp->setStyle(_style);
                qApp->setStyleSheet("");
            }
        }

        current_theme = theme;
    };
    internal();

    auto proxor_css = ReadFileText(":/proxor/proxor.css");
    qApp->setStyleSheet(qApp->styleSheet().append("\n").append(proxor_css));
}
