#ifndef PROXOR_DIALOG_VPN_SETTINGS_H
#define PROXOR_DIALOG_VPN_SETTINGS_H

#include <QDialog>
#include <QStringList>

class QEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogVPNSettings;
}
QT_END_NAMESPACE

class DialogVPNSettings : public QDialog {
    Q_OBJECT

public:
    explicit DialogVPNSettings(QWidget *parent = nullptr);

    ~DialogVPNSettings() override;

private:
    Ui::DialogVPNSettings *ui;

public slots:

    void accept() override;

    bool save(QStringList &flags);

    void on_troubleshooting_clicked();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void positionPickProcessButton();
};

#endif // PROXOR_DIALOG_VPN_SETTINGS_H
