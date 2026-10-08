#include "sys/wifi/WifiBackendChain.hpp"

#include <QDBusConnection>
#include <QElapsedTimer>
#include <QtTest>

using namespace ProxorWifi;

// The Linux factory is compiled here on every runner. It only constructs the backend and reads once
// (read-only queries); no service is registered and no state is changed.
std::unique_ptr<WifiBackend> CreatePlatformWifiBackend();

class WifiFactoryTest : public QObject {
    Q_OBJECT
private slots:
    void factoryBuildsTheChain() {
        auto backend = CreatePlatformWifiBackend();
        QVERIFY(backend != nullptr);
        QVERIFY(dynamic_cast<NmThenIwdWifiReader *>(backend.get()) != nullptr);
    }

    void oneReadKeepsTheContract() {
        auto backend = CreatePlatformWifiBackend();
        QVERIFY(backend != nullptr);
        QElapsedTimer timer;
        timer.start();
        const WifiReading reading = backend->read();
        QVERIFY2(timer.elapsed() < 6000, qPrintable(QStringLiteral("read took %1 ms").arg(timer.elapsed())));
        QVERIFY(!reading.source.isEmpty());
    }

    void noSystemBusTakesTheFastPath() {
        if (QDBusConnection::systemBus().isConnected()) QSKIP("a system bus is connected on this runner");
        auto backend = CreatePlatformWifiBackend();
        QVERIFY(backend != nullptr);
        QElapsedTimer timer;
        timer.start();
        const WifiReading reading = backend->read();
        QVERIFY2(timer.elapsed() < 3000, qPrintable(QStringLiteral("read took %1 ms").arg(timer.elapsed())));
        QVERIFY(reading.state == ReadState::Unavailable || reading.state == ReadState::PermissionNeeded);
    }
};

QTEST_GUILESS_MAIN(WifiFactoryTest)
#include "wifi_factory_test.moc"
