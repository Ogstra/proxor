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
    bool applying = false;

    [[nodiscard]] QList<ThemeOption> AvailableThemes() const;
    [[nodiscard]] QString NormalizeTheme(const QString &theme) const;
    void ApplyTheme(const QString &theme, bool force = false);
    void ReapplyTitleBar();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void themeChanged(const QString &themeName);

private:
    bool title_bar_dark = false;
    bool event_filter_installed = false;
};

extern ThemeManager *themeManager;
