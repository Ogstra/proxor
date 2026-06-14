#pragma once

#include <QDialog>
#include <QMenu>
#include <QTableWidget>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>

#include "3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp"
#include "main/ProxorGui.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogManageRoutes;
}
QT_END_NAMESPACE

class DialogManageRoutes : public QDialog {
    Q_OBJECT

public:
    explicit DialogManageRoutes(QWidget *parent = nullptr);

    ~DialogManageRoutes() override;

    [[nodiscard]] int activePageHeightHint() const;

signals:
    void activePageGeometryChanged();

private:
    Ui::DialogManageRoutes *ui;

    struct {
        QString custom_route;
        QString custom_route_global;
    } CACHE;

    QMenu *builtInSchemesMenu;
    Qv2ray::ui::widgets::AutoCompleteTextEdit *directDomainTxt;
    Qv2ray::ui::widgets::AutoCompleteTextEdit *proxyDomainTxt;
    Qv2ray::ui::widgets::AutoCompleteTextEdit *blockDomainTxt;
    //
    Qv2ray::ui::widgets::AutoCompleteTextEdit *directIPTxt;
    Qv2ray::ui::widgets::AutoCompleteTextEdit *blockIPTxt;
    Qv2ray::ui::widgets::AutoCompleteTextEdit *proxyIPTxt;
    //
    QTableWidget *hostsMapTable = nullptr;
    //
    struct DirectSiteRule {
        QList<int> groups;
        QList<int> profiles;
        QStringList sites;
    };
    QList<DirectSiteRule> directSiteRules;
    int directSitesCurrent = -1;
    QListWidget *directSitesList = nullptr;
    QPlainTextEdit *directSitesEditor = nullptr;
    QPushButton *directSitesTargetsBtn = nullptr;
    //
    ProxorGui::Routing routing_cn_lan = ProxorGui::Routing(1);
    ProxorGui::Routing routing_global = ProxorGui::Routing(0);
    //
    QString title_base;
    QString active_routing;

    void wrapTabPagesInScrollAreas();

    void buildDirectSitesTab();
    void loadDirectSiteRules();
    [[nodiscard]] QString serializedDirectSiteRules() const;
    void commitDirectSitesEditor();
    void refreshDirectSitesList();
    void refreshDirectSitesDetail();
    [[nodiscard]] QString directSiteRuleSummary(const DirectSiteRule &rule) const;

public slots:

    void accept() override;

    bool save(QStringList &flags);

    QList<QAction *> getBuiltInSchemes();

    QAction *schemeToAction(const QString &name, const ProxorGui::Routing &scheme);

    void UpdateDisplayRouting(ProxorGui::Routing *conf, bool qv);

    void SaveDisplayRouting(ProxorGui::Routing *conf);

    void on_load_save_clicked();
};
