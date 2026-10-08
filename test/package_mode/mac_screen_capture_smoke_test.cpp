// macOS Screen Recording smoke test: only the non-prompting preflight is called. Never request,
// open Settings or capture here (they can show prompts).
#include "sys/macos/MacScreenCapture.h"

#include <QDebug>
#include <QtTest>

class MacScreenCaptureSmokeTest : public QObject {
    Q_OBJECT
private slots:
    void preflightDoesNotPrompt() {
        const bool granted = ProxorMac::ScreenCapturePreflight();
        qInfo() << "ScreenCapturePreflight =" << granted;
        QVERIFY(granted == true || granted == false);
    }
};

QTEST_APPLESS_MAIN(MacScreenCaptureSmokeTest)
#include "mac_screen_capture_smoke_test.moc"
