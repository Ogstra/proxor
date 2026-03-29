#include "dialog_update_progress.h"
#include "rpc/gRPC.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

UpdateProgressDialog::UpdateProgressDialog(const QString &assetName, QWidget *parent)
    : QDialog(parent) {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Downloading Update"));
    setFixedSize(400, 150);

    auto *mainLayout = new QVBoxLayout(this);
    
    auto *titleLabel = new QLabel(tr("Downloading %1...").arg(assetName), this);
    mainLayout->addWidget(titleLabel);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    mainLayout->addWidget(progressBar);

    auto *infoLayout = new QHBoxLayout();
    labelSpeed = new QLabel(tr("Starting..."), this);
    labelSize = new QLabel(tr("0 / ? MB"), this);
    labelEta = new QLabel(tr("Calculating ETA..."), this);

    infoLayout->addWidget(labelSpeed);
    infoLayout->addStretch();
    infoLayout->addWidget(labelEta);
    infoLayout->addStretch();
    infoLayout->addWidget(labelSize);

    mainLayout->addLayout(infoLayout);

    pollTimer = new QTimer(this);
    pollTimer->setInterval(500);
    connect(pollTimer, &QTimer::timeout, this, &UpdateProgressDialog::onPollTick);
    
    lastPollTime = QDateTime::currentDateTime();
    pollTimer->start();
}

void UpdateProgressDialog::onPollTick() {
    bool ok;
    auto resp = ProxorGui_rpc::QueryUpdateProgress(&ok);
    if (!ok) return;

    qint64 total    = resp.progress_total();
    qint64 received = resp.progress_received();

    auto now = QDateTime::currentDateTime();
    double elapsed = lastPollTime.msecsTo(now) / 1000.0;
    if (elapsed > 0) {
        double speed = (received - lastReceivedBytes) / elapsed;  
        labelSpeed->setText(speed > 1e6
            ? tr("%1 MB/s").arg(speed / 1e6, 0, 'f', 1)
            : tr("%1 KB/s").arg(speed / 1e3, 0, 'f', 0));
        
        if (total > 0 && speed > 0) {
            int eta = (total - received) / speed;
            labelEta->setText(eta > 60
                ? tr("%1m %2s").arg(eta / 60).arg(eta % 60)
                : tr("%1s").arg(eta));
        }
    }
    
    lastReceivedBytes = received;
    lastPollTime = now;

    if (total > 0) {
        progressBar->setMaximum(100);
        progressBar->setValue((int)(received * 100 / total));
    } else if (total == -1) {
        progressBar->setMaximum(0); 
        labelEta->hide();
    }

    labelSize->setText(tr("%1 / %2 MB")
        .arg(received / 1e6, 0, 'f', 1)
        .arg(total > 0 ? QString::number(total / 1e6, 'f', 1) : tr("?")));

    if (resp.progress_complete() || !resp.error().empty()) {
        pollTimer->stop();
        if (resp.error().empty()) {
            emit downloadComplete();
        } else {
            labelSpeed->setText(tr("Error: %1").arg(resp.error().c_str()));
        }
        QTimer::singleShot(800, this, &QWidget::close);
    }
}
