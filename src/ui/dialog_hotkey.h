#pragma once

#include <QDialog>
#include "main/ProxorGui.hpp"

QT_BEGIN_NAMESPACE
namespace Ui {
    class DialogHotkey;
}
QT_END_NAMESPACE

class DialogHotkey : public QDialog {
    Q_OBJECT

public:
    explicit DialogHotkey(QWidget *parent = nullptr);

    ~DialogHotkey() override;

    bool save(QStringList &flags);

private:
    Ui::DialogHotkey *ui;
};
