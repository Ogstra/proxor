#include "MacHelperService.h"

#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMutex>
#include <QPointer>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <map>
#include <memory>

using namespace MacHelperWire;

namespace {

QByteArray line(const QJsonObject &obj) {
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject helloReply(qint64 id) {
    QJsonObject o;
    o[kKeyId] = double(id);
    o[kKeyOk] = true;
    o[kKeyProtocol] = kMacHelperProtocolVersion;
    o[kKeyBuild] = QStringLiteral("b-test");
    o[kKeySingBox] = QStringLiteral("sb-test");
    o[kKeyUid] = 501;
    return o;
}

QJsonObject okReply(qint64 id) {
    QJsonObject o;
    o[kKeyId] = double(id);
    o[kKeyOk] = true;
    return o;
}

QJsonObject eventObject(const char *name) {
    QJsonObject o;
    o[kKeyEvent] = QLatin1String(name);
    return o;
}

} // namespace

// Scripted helper in its own QThread: the worker blocks in waitForReadyRead(), so the server needs an
// event loop of its own. Every non-hello request is answered after delayMs (unless silent).
class FakeServer : public QObject {
    Q_OBJECT
public:
    std::atomic<int> delayMs{0};
    std::atomic<bool> silent{false};
    std::atomic<bool> closeAfterHello{false};
    std::atomic<int> connections{0};
    std::atomic<int> closed{0};
    std::atomic<int> hellos{0};

    QStringList commands() {
        QMutexLocker lock(&mutex_);
        return commands_;
    }

public slots:
    bool listen(const QString &name) {
        server_ = new QLocalServer(this);
        server_->setSocketOptions(QLocalServer::NoOptions);
        QLocalServer::removeServer(name);
        connect(server_, &QLocalServer::newConnection, this, &FakeServer::onNewConnection);
        return server_->listen(name);
    }
    void shutdown() {
        if (server_) {
            server_->close();
            delete server_;
            server_ = nullptr;
        }
        buffers_.clear();
    }
    void pushEvents() {
        for (auto &entry : buffers_) {
            QLocalSocket *s = entry.first;
            QJsonObject ready = eventObject(kEventTunReady);
            QJsonObject logEv = eventObject(kEventLog);
            logEv[kKeyLine] = QStringLiteral("hello log");
            QJsonObject stopped = eventObject(kEventTunStopped);
            stopped[kKeyReason] = QStringLiteral("because");
            s->write(line(ready));
            s->write(line(logEv));
            s->write(line(stopped));
        }
    }

private:
    void onNewConnection() {
        while (QLocalSocket *s = server_->nextPendingConnection()) {
            ++connections;
            s->setParent(server_);
            buffers_[s] = QByteArray();
            connect(s, &QLocalSocket::disconnected, this, [this, s] {
                ++closed;
                buffers_.erase(s);
            });
            connect(s, &QLocalSocket::readyRead, this, [this, s] { onReadyRead(s); });
        }
    }
    void onReadyRead(QLocalSocket *s) {
        auto it = buffers_.find(s);
        if (it == buffers_.end()) return;
        it->second.append(s->readAll());
        for (;;) {
            it = buffers_.find(s);
            if (it == buffers_.end()) return;
            const int nl = it->second.indexOf('\n');
            if (nl < 0) return;
            const QByteArray l = it->second.left(nl);
            it->second.remove(0, nl + 1);
            const QJsonDocument doc = QJsonDocument::fromJson(l);
            if (doc.isObject()) dispatch(s, doc.object());
        }
    }
    void dispatch(QLocalSocket *s, const QJsonObject &req) {
        const qint64 id = qint64(req.value(kKeyId).toDouble());
        const QString cmd = req.value(kKeyCmd).toString();
        if (cmd == QLatin1String(kCmdHello)) {
            ++hellos;
            s->write(line(helloReply(id)));
            if (closeAfterHello) s->disconnectFromServer();
            return;
        }
        {
            QMutexLocker lock(&mutex_);
            commands_ << cmd;
        }
        if (silent) return;
        QPointer<QLocalSocket> guard(s);
        QTimer::singleShot(delayMs.load(), this, [guard, id] {
            if (guard) guard->write(line(okReply(id)));
        });
    }

    QLocalServer *server_ = nullptr;
    std::map<QLocalSocket *, QByteArray> buffers_;
    QMutex mutex_;
    QStringList commands_;
};

class MacHelperServiceTest final : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmp_;
    QThread thread_;
    FakeServer *server_ = nullptr;
    MacHelperClient::Paths paths_;
    int counter_ = 0;

    QString uniqueSocketName() {
        const QString leaf = QStringLiteral("mhs-%1-%2").arg(QCoreApplication::applicationPid()).arg(++counter_);
#ifdef Q_OS_WIN
        return leaf;
#else
        return QDir::tempPath() + QLatin1Char('/') + leaf;
#endif
    }

    void touch(const QString &path) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("x");
    }

    void startServer() {
        server_ = new FakeServer();
        server_->moveToThread(&thread_);
        thread_.start();
        paths_.socket = uniqueSocketName();
        bool listening = false;
        QMetaObject::invokeMethod(server_, "listen", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, listening),
                                  Q_ARG(QString, paths_.socket));
        QVERIFY(listening);
        paths_.plist = tmp_.filePath(QStringLiteral("helper.plist"));
        paths_.binary = tmp_.filePath(QStringLiteral("helper-bin"));
        touch(paths_.plist);
        touch(paths_.binary);
    }

private slots:
    void init() {
        QFile::remove(tmp_.filePath(QStringLiteral("helper.plist")));
        QFile::remove(tmp_.filePath(QStringLiteral("helper-bin")));
        paths_ = MacHelperClient::Paths();
    }

    void cleanup() {
        if (!server_) return;
        QMetaObject::invokeMethod(server_, "shutdown", Qt::BlockingQueuedConnection);
        thread_.quit();
        thread_.wait();
        delete server_;
        server_ = nullptr;
    }

    void returnsImmediately() {
        startServer();
        server_->delayMs = 2000;
        MacHelperService svc(paths_);
        QObject ctx;

        qint64 maxGap = 0;
        QElapsedTimer sinceTick;
        QTimer ticker;
        ticker.setInterval(10);
        connect(&ticker, &QTimer::timeout, this, [&] {
            maxGap = qMax(maxGap, sinceTick.elapsed());
            sinceTick.restart();
        });

        bool done = false;
        bool ok = false;
        QElapsedTimer call;
        call.start();
        sinceTick.start();
        ticker.start();
        svc.sysproxyRestore(&ctx, 20000, [&](const MacHelperService::Reply &r) {
            ok = r.ok;
            done = true;
        });
        QVERIFY2(call.elapsed() < 500, "the call returned late");
        QVERIFY(!done);
        QTRY_VERIFY_WITH_TIMEOUT(done, 15000);
        ticker.stop();
        QVERIFY(ok);
        QVERIFY2(maxGap < 500, qPrintable(QStringLiteral("event loop stalled for %1 ms").arg(maxGap)));
    }

    void callbacksOnCallerThread() {
        startServer();
        MacHelperService svc(paths_);
        QObject ctx;
        QThread *const self = QThread::currentThread();
        std::atomic<int> wrongThread{0};
        int events = 0;
        connect(&svc, &MacHelperService::tunReady, this, [&] {
            if (QThread::currentThread() != self) ++wrongThread;
            ++events;
        });
        connect(&svc, &MacHelperService::helperLog, this, [&](const QString &) {
            if (QThread::currentThread() != self) ++wrongThread;
            ++events;
        });
        connect(&svc, &MacHelperService::tunStopped, this, [&](const QString &) {
            if (QThread::currentThread() != self) ++wrongThread;
            ++events;
        });

        int callbacks = 0;
        svc.status(&ctx, 5000, [&](const MacHelperService::Reply &) {
            if (QThread::currentThread() != self) ++wrongThread;
            ++callbacks;
        });
        svc.probe(&ctx, 2000, [&](const MacHelperProbe &, MacHelperState) {
            if (QThread::currentThread() != self) ++wrongThread;
            ++callbacks;
        });
        QTRY_COMPARE_WITH_TIMEOUT(callbacks, 2, 10000);
        QMetaObject::invokeMethod(server_, "pushEvents", Qt::BlockingQueuedConnection);
        // The events are read on the worker's next request (the client only polls inside requests).
        svc.status(&ctx, 5000, [&](const MacHelperService::Reply &) { ++callbacks; });
        QTRY_COMPARE_WITH_TIMEOUT(callbacks, 3, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(events, 3, 10000);
        QCOMPARE(wrongThread.load(), 0);
    }

    void fifoOrder() {
        startServer();
        server_->delayMs = 50;
        MacHelperService svc(paths_);
        QObject ctx;
        QList<int> order;
        svc.tunStart(&ctx, QByteArrayLiteral("{}"), 1080, 10000, [&](const MacHelperService::Reply &) { order << 1; });
        svc.sysproxyApply(&ctx, 1080, QStringList(), 10000, [&](const MacHelperService::Reply &) { order << 2; });
        svc.sysproxyRestore(&ctx, 10000, [&](const MacHelperService::Reply &) { order << 3; });
        svc.tunStop(&ctx, 10000, [&](const MacHelperService::Reply &) { order << 4; });
        QTRY_COMPARE_WITH_TIMEOUT(order.size(), 4, 15000);
        QTest::qWait(200);
        QCOMPARE(order, (QList<int>{1, 2, 3, 4}));
        const QStringList expected{QLatin1String(kCmdTunStart), QLatin1String(kCmdSysproxyApply),
                                   QLatin1String(kCmdSysproxyRestore), QLatin1String(kCmdTunStop)};
        QCOMPARE(server_->commands(), expected);
        QCOMPARE(svc.pending(), 0);
    }

    void deadContextDropped() {
        startServer();
        server_->delayMs = 500;
        MacHelperService svc(paths_);
        bool called = false;
        {
            auto *dead = new QObject;
            svc.status(dead, 5000, [&](const MacHelperService::Reply &) { called = true; });
            delete dead;
        }
        QTRY_COMPARE_WITH_TIMEOUT(svc.pending(), 0, 10000);
        QTest::qWait(300);
        QVERIFY(!called);

        QObject ctx;
        bool ok = false;
        bool done = false;
        svc.status(&ctx, 5000, [&](const MacHelperService::Reply &r) {
            ok = r.ok;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QVERIFY(ok);
        QVERIFY(!called);
    }

    void probeClassifies() {
        // nothing installed
        {
            MacHelperClient::Paths absent;
            absent.socket = uniqueSocketName();
            absent.plist = tmp_.filePath(QStringLiteral("absent.plist"));
            absent.binary = tmp_.filePath(QStringLiteral("absent-bin"));
            MacHelperService svc(absent);
            QCOMPARE(svc.lastState(), MacHelperState::NotInstalled);
            QObject ctx;
            bool done = false;
            MacHelperState got = MacHelperState::Ready;
            svc.probe(&ctx, 1000, [&](const MacHelperProbe &, MacHelperState s) {
                got = s;
                done = true;
            });
            QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
            QCOMPARE(got, MacHelperState::NotInstalled);
            QVERIFY(!svc.isConnected());
        }
        // fake Ready
        startServer();
        MacHelperService svc(paths_);
        QObject ctx;
        bool done = false;
        bool cachedConnected = false;
        MacHelperState cachedState = MacHelperState::NotInstalled;
        MacHelperState got = MacHelperState::NotInstalled;
        QString build;
        QString singbox;
        svc.probe(&ctx, 2000, [&](const MacHelperProbe &p, MacHelperState s) {
            got = s;
            build = p.build;
            singbox = p.singbox;
            cachedConnected = svc.isConnected();
            cachedState = svc.lastState();
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QCOMPARE(got, MacHelperState::Ready);
        QCOMPARE(build, QStringLiteral("b-test"));
        QCOMPARE(singbox, QStringLiteral("sb-test"));
        QVERIFY(cachedConnected);
        QCOMPARE(cachedState, MacHelperState::Ready);
    }

    void eventsForwarded() {
        startServer();
        MacHelperService svc(paths_);
        QObject ctx;
        int ready = 0;
        QStringList logs;
        QStringList stops;
        connect(&svc, &MacHelperService::tunReady, this, [&] { ++ready; });
        connect(&svc, &MacHelperService::helperLog, this, [&](const QString &l) { logs << l; });
        connect(&svc, &MacHelperService::tunStopped, this, [&](const QString &r) { stops << r; });

        bool connected = false;
        svc.probe(&ctx, 2000, [&](const MacHelperProbe &, MacHelperState) { connected = true; });
        QTRY_VERIFY_WITH_TIMEOUT(connected, 10000);
        QMetaObject::invokeMethod(server_, "pushEvents", Qt::BlockingQueuedConnection);
        // Events are picked up by the socket's readyRead on the worker's idle event loop.
        QTRY_COMPARE_WITH_TIMEOUT(ready, 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(logs.size(), 1, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(stops.size(), 1, 10000);
        QCOMPARE(logs.first(), QStringLiteral("hello log"));
        QCOMPARE(stops.first(), QStringLiteral("because"));
    }

    void connectionLostForwarded() {
        startServer();
        server_->closeAfterHello = true;
        MacHelperService svc(paths_);
        QObject ctx;
        int lost = 0;
        connect(&svc, &MacHelperService::connectionLost, this, [&] { ++lost; });

        bool done = false;
        svc.probe(&ctx, 2000, [&](const MacHelperProbe &, MacHelperState) { done = true; });
        QTRY_VERIFY_WITH_TIMEOUT(done, 10000);
        QTRY_COMPARE_WITH_TIMEOUT(lost, 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!svc.isConnected(), 10000);
        QTest::qWait(300);
        QCOMPARE(lost, 1);

        // the next call reconnects
        server_->closeAfterHello = false;
        bool ok = false;
        bool answered = false;
        svc.status(&ctx, 5000, [&](const MacHelperService::Reply &r) {
            ok = r.ok;
            answered = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(answered, 10000);
        QVERIFY(ok);
        QVERIFY(svc.isConnected());
        QVERIFY(server_->hellos.load() >= 2);
    }

    void timeoutReported() {
        startServer();
        server_->silent = true;
        MacHelperService svc(paths_);
        QObject ctx;
        bool done = false;
        bool ok = true;
        QString error;
        QElapsedTimer t;
        t.start();
        qint64 elapsed = 0;
        svc.status(&ctx, 1000, [&](const MacHelperService::Reply &r) {
            ok = r.ok;
            error = r.error;
            elapsed = t.elapsed();
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 1000 + 3000);
        QVERIFY(!ok);
        QCOMPARE(error, QStringLiteral("timeout"));
        QVERIFY2(elapsed >= 900, qPrintable(QStringLiteral("timed out after only %1 ms").arg(elapsed)));
    }

    void shutdownBounded() {
        startServer();
        MacHelperService svc(paths_);
        QObject ctx;
        bool connected = false;
        svc.probe(&ctx, 2000, [&](const MacHelperProbe &, MacHelperState) { connected = true; });
        QTRY_VERIFY_WITH_TIMEOUT(connected, 10000);
        QVERIFY(svc.isConnected());

        QElapsedTimer t;
        t.start();
        QVERIFY(svc.shutdown(2000));
        QVERIFY2(t.elapsed() < 1500, qPrintable(QStringLiteral("shutdown took %1 ms").arg(t.elapsed())));
        QTRY_VERIFY_WITH_TIMEOUT(server_->closed.load() >= 1, 5000);
        QVERIFY(svc.shutdown(2000)); // idempotent

        bool done = false;
        bool ok = true;
        QString error;
        svc.sysproxyRestore(&ctx, 20000, [&](const MacHelperService::Reply &r) {
            ok = r.ok;
            error = r.error;
            done = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(done, 5000);
        QVERIFY(!ok);
        QCOMPARE(error, QStringLiteral("the Proxor service client is shut down"));
    }
};

QTEST_MAIN(MacHelperServiceTest)
#include "mac_helper_service_test.moc"
