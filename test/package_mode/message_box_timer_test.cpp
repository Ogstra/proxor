#include <QApplication>
#include <QMessageBox>
#include <QPointer>
#include <QTest>

#include "ui/widget/MessageBoxTimer.h"

// The "restart the software" prompt must never trap the user: it is non-modal, and a cancel()
// that races the (queued) timeout always wins.
class MessageBoxTimerTest : public QObject {
    Q_OBJECT
private slots:
    void showsNonModalAfterDelayAndCancelHidesIt() {
        QWidget parent;
        auto *box = new QMessageBox(QMessageBox::Question, "t", "m", QMessageBox::Yes | QMessageBox::No, &parent);
        auto *timer = new MessageBoxTimer(&parent, box, 50);
        QVERIFY(!box->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(box->isVisible(), 2000); // event loop keeps spinning: exec() would never return here
        QCOMPARE(box->windowModality(), Qt::NonModal);
        timer->cancel();
        QVERIFY(!box->isVisible());
    }

    void cancelBeforeTimeoutNeverShows() {
        QWidget parent;
        auto *box = new QMessageBox(QMessageBox::Question, "t", "m", QMessageBox::Yes | QMessageBox::No, &parent);
        auto *timer = new MessageBoxTimer(&parent, box, 20);
        timer->cancel();
        QTest::qWait(150);
        QVERIFY(!box->isVisible());
    }

    void cancelAfterTimeoutQueuedButBeforeSlotRuns() {
        QWidget parent;
        auto *box = new QMessageBox(QMessageBox::Question, "t", "m", QMessageBox::Yes | QMessageBox::No, &parent);
        auto *timer = new MessageBoxTimer(&parent, box, 1);
        QTest::qSleep(30); // timeout is due but not yet delivered
        timer->cancel();
        QTest::qWait(100);
        QVERIFY(!box->isVisible());
    }

    void deletedBoxIsSafe() {
        QWidget parent;
        auto *box = new QMessageBox(QMessageBox::Question, "t", "m", QMessageBox::Yes | QMessageBox::No, &parent);
        auto *timer = new MessageBoxTimer(&parent, box, 10);
        delete box;
        QTest::qWait(100);
        timer->cancel();
    }
};

QTEST_MAIN(MessageBoxTimerTest)
#include "message_box_timer_test.moc"
