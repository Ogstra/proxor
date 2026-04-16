#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class ThemeManager : public QObject {
    Q_OBJECT
public:
    using ThemeOption = QPair<QString, QString>;

    QString system_style_name = "";
    QString current_theme = "System";

    [[nodiscard]] QList<ThemeOption> AvailableThemes() const;
    [[nodiscard]] QString NormalizeTheme(const QString &theme) const;
    void ApplyTheme(const QString &theme, bool force = false);

signals:
    void themeChanged(const QString &themeName);
};

extern ThemeManager *themeManager;
