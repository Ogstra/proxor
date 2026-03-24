#pragma once

#include <QObject>
#include <QString>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    QString system_style_name = "";
    QString current_theme = "System";

    void ApplyTheme(const QString &theme, bool force = false);

signals:
    void themeChanged(const QString &themeName);
};

extern ThemeManager *themeManager;
