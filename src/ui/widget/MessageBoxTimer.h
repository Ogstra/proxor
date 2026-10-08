#pragma once

#include <QMessageBox>
#include <QPointer>
#include <QTimer>

// Shows `msgbox` after `delayMs` unless cancel() came first. The box is NON-modal: a modal exec() here froze
// every other control (Stop included) behind a "restart the software" prompt whenever a start took longer than
// the delay. cancel() always wins, even if the timeout was already queued.
class MessageBoxTimer : public QTimer {
public:
    QPointer<QMessageBox> msgbox;
    bool showed = false;

    explicit MessageBoxTimer(QObject *parent, QMessageBox *msgbox, int delayMs) : QTimer(parent) {
        connect(this, &QTimer::timeout, this, &MessageBoxTimer::timeoutFunc, Qt::ConnectionType::QueuedConnection);
        this->msgbox = msgbox;
        setSingleShot(true);
        setInterval(delayMs);
        start();
    };

    void cancel() {
        cancelled = true;
        QTimer::stop();
        if (msgbox != nullptr && showed) {
            msgbox->reject(); // hides the non-modal box
        }
    };

private:
    bool cancelled = false;

    void timeoutFunc() {
        if (cancelled || msgbox == nullptr) return;
        showed = true;
        msgbox->setWindowModality(Qt::NonModal);
        msgbox->show();
    }
};
