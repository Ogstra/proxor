#include "MacHelperClient.h"

#include <QtTest>

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <functional>
#include <map>

using namespace MacHelperWire;

namespace {

QByteArray line(const QJsonObject &obj) {
    return QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject helloReply(qint64 id, int protocol = 1) {
    QJsonObject o;
    o[kKeyId] = double(id);
    o[kKeyOk] = true;
    o[kKeyProtocol] = protocol;
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

} // namespace

// Scripted helper. Lives in its own QThread with its own event loop: the client blocks its calling
// thread in waitForReadyRead(), so a server on the same thread could never accept or answer.
class FakeServer : public QObject {
    Q_OBJECT
public:
    using Accept = std::function<void(FakeServer &, QLocalSocket *)>;
    using Handler = std::function<void(FakeServer &, QLocalSocket *, const QJsonObject &)>;

    FakeServer(Handler handler, Accept accept) : handler_(std::move(handler)), accept_(std::move(accept)) {}

    std::atomic<int> connections{0};
    std::atomic<int> hellos{0};

    // Answers hello like the real helper (protocol 1); everything else goes to the handler.
    void dispatch(QLocalSocket *s, const QJsonObject &req) {
        const qint64 id = qint64(req.value(kKeyId).toDouble());
        if (req.value(kKeyCmd).toString() == QLatin1String(kCmdHello)) {
            ++hellos;
            if (helloOverride_) {
                helloOverride_(*this, s, req);
                return;
            }
            s->write(line(helloReply(id)));
            return;
        }
        if (handler_) handler_(*this, s, req);
    }
    Handler helloOverride_;

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

private:
    void onNewConnection() {
        while (QLocalSocket *s = server_->nextPendingConnection()) {
            ++connections;
            s->setParent(server_);
            buffers_[s] = QByteArray();
            connect(s, &QLocalSocket::disconnected, this, [this, s] { buffers_.erase(s); });
            connect(s, &QLocalSocket::readyRead, this, [this, s] { onReadyRead(s); });
            if (accept_) accept_(*this, s);
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

    Handler handler_;
    Accept accept_;
    QLocalServer *server_ = nullptr;
    std::map<QLocalSocket *, QByteArray> buffers_;
};

class MacHelperClientTest final : public QObject {
    Q_OBJECT

private:
    QTemporaryDir tmp_;
    QThread thread_;
    FakeServer *server_ = nullptr;
    MacHelperClient::Paths paths_;
    int counter_ = 0;

    QString uniqueSocketName() {
        const QString leaf = QStringLiteral("mh-%1-%2").arg(QCoreApplication::applicationPid()).arg(++counter_);
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

    void startServer(FakeServer::Handler handler, FakeServer::Accept accept = {}, bool withFiles = true,
                     FakeServer::Handler helloOverride = {}) {
        server_ = new FakeServer(std::move(handler), std::move(accept));
        server_->helloOverride_ = std::move(helloOverride);
        server_->moveToThread(&thread_);
        thread_.start();
        paths_.socket = uniqueSocketName();
        bool listening = false;
        QMetaObject::invokeMethod(server_, "listen", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, listening),
                                  Q_ARG(QString, paths_.socket));
        QVERIFY(listening);
        paths_.plist = tmp_.filePath(QStringLiteral("helper.plist"));
        paths_.binary = tmp_.filePath(QStringLiteral("helper-bin"));
        if (withFiles) {
            touch(paths_.plist);
            touch(paths_.binary);
        }
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

    void probeClosedWithoutReplyIsNotAuthorized() {
        startServer({}, [](FakeServer &, QLocalSocket *s) { s->disconnectFromServer(); });
        MacHelperClient client(paths_);
        const MacHelperProbe p = client.probe(1000);
        QVERIFY(p.connected);
        QVERIFY(!p.helloOk);
        QVERIFY(p.rejected);
        QCOMPARE(ClassifyMacHelper(p), MacHelperState::NotAuthorized);
        QVERIFY(!client.isConnected());
    }

    void probeNothingInstalledIsNotInstalled() {
        paths_.socket = uniqueSocketName();
        paths_.plist = tmp_.filePath(QStringLiteral("absent.plist"));
        paths_.binary = tmp_.filePath(QStringLiteral("absent-bin"));
        MacHelperClient client(paths_);
        const MacHelperProbe p = client.probe(1000);
        QVERIFY(!p.plistPresent);
        QVERIFY(!p.binaryPresent);
        QVERIFY(!p.connected);
        QCOMPARE(ClassifyMacHelper(p), MacHelperState::NotInstalled);
    }

    void probeFilesButNoServerIsNotRunning() {
        paths_.socket = uniqueSocketName();
        paths_.plist = tmp_.filePath(QStringLiteral("helper.plist"));
        paths_.binary = tmp_.filePath(QStringLiteral("helper-bin"));
        touch(paths_.plist);
        touch(paths_.binary);
        MacHelperClient client(paths_);
        QCOMPARE(client.state(1000), MacHelperState::InstalledNotRunning);
    }

    void probeAnsweringHelperIsReady() {
        startServer({});
        MacHelperClient client(paths_);
        const MacHelperProbe p = client.probe(1000);
        QVERIFY(p.plistPresent);
        QVERIFY(p.binaryPresent);
        QVERIFY(p.connected);
        QVERIFY(p.helloOk);
        QCOMPARE(p.protocol, 1);
        QCOMPARE(p.build, QStringLiteral("b-test"));
        QCOMPARE(p.singbox, QStringLiteral("sb-test"));
        QCOMPARE(ClassifyMacHelper(p), MacHelperState::Ready);
        QVERIFY(client.isConnected());
        // Probing again reuses the persistent connection (the lease), no second hello.
        QCOMPARE(client.state(1000), MacHelperState::Ready);
        QCOMPARE(server_->connections.load(), 1);
        QCOMPARE(server_->hellos.load(), 1);
    }

    void probeProtocolMismatchIsOutdated() {
        startServer({}, {}, true, [](FakeServer &, QLocalSocket *s, const QJsonObject &req) {
            QJsonObject o;
            o[kKeyId] = req.value(kKeyId);
            o[kKeyOk] = false;
            o[kKeyError] = QStringLiteral("protocol mismatch");
            o[kKeyProtocol] = 2;
            s->write(line(o));
            s->disconnectFromServer();
        });
        MacHelperClient client(paths_);
        QSignalSpy lost(&client, &MacHelperClient::connectionLost);
        const MacHelperProbe p = client.probe(1000);
        QVERIFY(p.connected);
        QVERIFY(!p.helloOk);
        QCOMPARE(p.protocol, 2);
        QCOMPARE(ClassifyMacHelper(p), MacHelperState::Outdated);
        QVERIFY(!client.isConnected());
        QTest::qWait(100);
        QCOMPARE(lost.count(), 0); // a failed hello is not a lost session
    }

    void probeSilentServerTimesOut() {
        startServer({}, {}, true, [](FakeServer &, QLocalSocket *, const QJsonObject &) { /* never answers */ });
        MacHelperClient client(paths_);
        QElapsedTimer t;
        t.start();
        const MacHelperProbe p = client.probe(300);
        QVERIFY2(t.elapsed() < 500, qPrintable(QString::number(t.elapsed())));
        QVERIFY(p.connected);
        QVERIFY(!p.helloOk);
        QVERIFY(!p.rejected);
        QCOMPARE(ClassifyMacHelper(p), MacHelperState::InstalledNotRunning);
    }

    void requestReturnsMatchingReplyAndEmitsEvents() {
        startServer([](FakeServer &, QLocalSocket *s, const QJsonObject &req) {
            const qint64 id = qint64(req.value(kKeyId).toDouble());
            QJsonObject log;
            log[kKeyEvent] = QLatin1String(kEventLog);
            log[kKeyLine] = QStringLiteral("a");
            s->write(line(log));
            QJsonObject ready;
            ready[kKeyEvent] = QLatin1String(kEventTunReady);
            s->write(line(ready));
            QJsonObject stale = okReply(id - 1000); // a reply nobody waits for must be ignored
            s->write(line(stale));
            QJsonObject r = okReply(id);
            r[kKeyTunRunning] = true;
            s->write(line(r));
            QJsonObject stopped;
            stopped[kKeyEvent] = QLatin1String(kEventTunStopped);
            stopped[kKeyReason] = QStringLiteral("gone");
            s->write(line(stopped));
        });
        MacHelperClient client(paths_);
        QSignalSpy logSpy(&client, &MacHelperClient::helperLog);
        QSignalSpy readySpy(&client, &MacHelperClient::tunReady);
        QSignalSpy stoppedSpy(&client, &MacHelperClient::tunStopped);
        const MacHelperClient::Reply r = client.status(2000);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.body.value(kKeyTunRunning).toBool(), true);
        QTRY_COMPARE(logSpy.count(), 1);
        QCOMPARE(logSpy.at(0).at(0).toString(), QStringLiteral("a"));
        QTRY_COMPARE(readySpy.count(), 1);
        // The event that arrived right behind the reply is not held back until the next request.
        QTRY_COMPARE(stoppedSpy.count(), 1);
        QCOMPARE(stoppedSpy.at(0).at(0).toString(), QStringLiteral("gone"));
    }

    void replySplitAcrossWritesIsReassembled() {
        startServer([](FakeServer &, QLocalSocket *s, const QJsonObject &req) {
            const QByteArray whole = line(okReply(qint64(req.value(kKeyId).toDouble())));
            s->write(whole.left(5));
            QTimer::singleShot(60, s, [s, whole] { s->write(whole.mid(5)); });
        });
        MacHelperClient client(paths_);
        const MacHelperClient::Reply r = client.status(2000);
        QVERIFY2(r.ok, qPrintable(r.error));
    }

    void requestBodiesFollowTheWireFormat() {
        startServer([](FakeServer &, QLocalSocket *s, const QJsonObject &req) {
            QJsonObject r = okReply(qint64(req.value(kKeyId).toDouble()));
            r[QStringLiteral("echo")] = req;
            s->write(line(r));
        });
        MacHelperClient client(paths_);

        MacHelperClient::Reply r = client.tunStart(QByteArray("{\"log\":{}}"), 2080, 2000);
        QVERIFY2(r.ok, qPrintable(r.error));
        QJsonObject echo = r.body.value(QStringLiteral("echo")).toObject();
        QCOMPARE(echo.value(kKeyCmd).toString(), QStringLiteral("tun_start"));
        QCOMPARE(echo.value(kKeyConfig).toString(), QStringLiteral("{\"log\":{}}"));
        QCOMPARE(echo.value(kKeySocksPort).toInt(), 2080);

        r = client.sysproxyApply(2080, {QStringLiteral("localhost"), QStringLiteral("10.0.0.0/8")}, 2000);
        QVERIFY2(r.ok, qPrintable(r.error));
        echo = r.body.value(QStringLiteral("echo")).toObject();
        QCOMPARE(echo.value(kKeyCmd).toString(), QStringLiteral("sysproxy_apply"));
        QCOMPARE(echo.value(kKeyPort).toInt(), 2080);
        QCOMPARE(echo.value(kKeyBypass).toArray().size(), 2);

        r = client.tunStop(2000);
        QCOMPARE(r.body.value(QStringLiteral("echo")).toObject().value(kKeyCmd).toString(), QStringLiteral("tun_stop"));
        r = client.sysproxyRestore(2000);
        QCOMPARE(r.body.value(QStringLiteral("echo")).toObject().value(kKeyCmd).toString(), QStringLiteral("sysproxy_restore"));
        r = client.uninstall(2000);
        QCOMPARE(r.body.value(QStringLiteral("echo")).toObject().value(kKeyCmd).toString(), QStringLiteral("uninstall"));
        QCOMPARE(server_->connections.load(), 1); // one persistent connection for all of it
    }

    void helperErrorIsSurfaced() {
        startServer([](FakeServer &, QLocalSocket *s, const QJsonObject &req) {
            QJsonObject r;
            r[kKeyId] = req.value(kKeyId);
            r[kKeyOk] = false;
            r[kKeyError] = QStringLiteral("tun refused");
            s->write(line(r));
        });
        MacHelperClient client(paths_);
        const MacHelperClient::Reply r = client.tunStop(2000);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, QStringLiteral("tun refused"));
    }

    void requestTimeout() {
        startServer([](FakeServer &, QLocalSocket *, const QJsonObject &) { /* ignores every command */ });
        MacHelperClient client(paths_);
        QElapsedTimer t;
        t.start();
        const MacHelperClient::Reply r = client.status(300);
        QVERIFY(!r.ok);
        QCOMPARE(r.error, QStringLiteral("timeout"));
        QVERIFY(t.elapsed() >= 250);
        QVERIFY(t.elapsed() < 1500);
        QVERIFY(client.isConnected()); // a slow command does not drop the lease
    }

    void requestWithoutHelperFails() {
        paths_.socket = uniqueSocketName();
        MacHelperClient client(paths_);
        const MacHelperClient::Reply r = client.status(500);
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
    }

    void connectionLostIsEmittedOnceAndNextRequestReconnects() {
        startServer(
            [](FakeServer &, QLocalSocket *s, const QJsonObject &req) {
                s->write(line(okReply(qint64(req.value(kKeyId).toDouble()))));
            },
            {}, true,
            [](FakeServer &self, QLocalSocket *s, const QJsonObject &req) {
                s->write(line(helloReply(qint64(req.value(kKeyId).toDouble()))));
                if (self.hellos.load() == 1) s->disconnectFromServer(); // only the first session dies
            });
        MacHelperClient client(paths_);
        QSignalSpy lost(&client, &MacHelperClient::connectionLost);
        QCOMPARE(client.state(1000), MacHelperState::Ready);
        QTRY_COMPARE(lost.count(), 1);
        QTest::qWait(150);
        QCOMPARE(lost.count(), 1);
        QVERIFY(!client.isConnected());

        const MacHelperClient::Reply r = client.status(2000);
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(server_->hellos.load(), 2);
        QCOMPARE(server_->connections.load(), 2);
        QVERIFY(client.isConnected());
        QCOMPARE(lost.count(), 1);
    }

    void deliberateDisconnectIsNotALoss() {
        startServer({});
        MacHelperClient client(paths_);
        QSignalSpy lost(&client, &MacHelperClient::connectionLost);
        QCOMPARE(client.state(1000), MacHelperState::Ready);
        client.disconnectFromHelper();
        QVERIFY(!client.isConnected());
        QTest::qWait(150);
        QCOMPARE(lost.count(), 0);
    }

    void serverDyingDuringRequestFailsTheRequestAndEmitsLossOnce() {
        startServer([](FakeServer &, QLocalSocket *s, const QJsonObject &) { s->disconnectFromServer(); });
        MacHelperClient client(paths_);
        QSignalSpy lost(&client, &MacHelperClient::connectionLost);
        const MacHelperClient::Reply r = client.status(2000);
        QVERIFY(!r.ok);
        QVERIFY(!r.error.isEmpty());
        QTRY_COMPARE(lost.count(), 1);
        QTest::qWait(100);
        QCOMPARE(lost.count(), 1);
    }

    void oversizedLineDropsTheConnection() {
        startServer([](FakeServer &, QLocalSocket *s, const QJsonObject &) {
            s->write(QByteArray(1024 * 1024 + 4096, 'x')); // no newline: never a valid line
        });
        MacHelperClient client(paths_);
        QSignalSpy lost(&client, &MacHelperClient::connectionLost);
        const MacHelperClient::Reply r = client.status(3000);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("too long")));
        QVERIFY(!client.isConnected());
        QTRY_COMPARE(lost.count(), 1);
    }

    void adminAppleScriptQuoting() {
        const QString got = MacAdminAppleScript(QStringLiteral("/bin/sh"),
                                                {QStringLiteral("/A B/it's \"x\".sh"), QStringLiteral("501")});
        // Shell layer: '/bin/sh' '/A B/it'"'"'s "x".sh' '501'; AppleScript layer escapes every " as \".
        const QString expected = QStringLiteral(
            R"(do shell script "'/bin/sh' '/A B/it'\"'\"'s \"x\".sh' '501'" with administrator privileges)");
        QCOMPARE(got, expected);
    }

    void adminAppleScriptEscapesBackslashBeforeQuote() {
        const QString got = MacAdminAppleScript(QStringLiteral("/bin/sh"), {QStringLiteral("a\\b\"c")});
        const QString expected = QStringLiteral(
            R"(do shell script "'/bin/sh' 'a\\b\"c'" with administrator privileges)");
        QCOMPARE(got, expected);
    }

    void shellQuoteHandlesApostrophes() {
        QCOMPARE(MacShellQuote(QStringLiteral("plain")), QStringLiteral("'plain'"));
        QCOMPARE(MacShellQuote(QStringLiteral("it's")), QStringLiteral("'it'\"'\"'s'"));
        QCOMPARE(MacShellQuote(QString()), QStringLiteral("''"));
    }

    void defaultPathsUseTheWireConstants() {
        const MacHelperClient::Paths p = MacHelperClient::DefaultPaths();
        QCOMPARE(p.socket, QStringLiteral("/var/run/io.github.Ogstra.Proxor.helper.sock"));
        QCOMPARE(p.plist, QStringLiteral("/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist"));
        QCOMPARE(p.binary, QStringLiteral("/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper"));
    }
};

QTEST_MAIN(MacHelperClientTest)
#include "mac_helper_client_test.moc"
