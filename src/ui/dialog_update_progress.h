#ifndef DIALOG_UPDATE_PROGRESS_H
#define DIALOG_UPDATE_PROGRESS_H

#include <QDialog>
#include <QProgressBar>
#include <QLabel>
#include <QTimer>
#include <QDateTime>

class UpdateProgressDialog : public QDialog {
    Q_OBJECT
public:
    explicit UpdateProgressDialog(const QString &assetName, QWidget *parent = nullptr);

signals:
    void downloadComplete();

private slots:
    void onPollTick();

private:
    QProgressBar *progressBar;
    QLabel       *labelSpeed;
    QLabel       *labelSize;
    QLabel       *labelEta;
    QTimer       *pollTimer;
    qint64        lastReceivedBytes = 0;
    QDateTime     lastPollTime;
};

#endif // DIALOG_UPDATE_PROGRESS_H
