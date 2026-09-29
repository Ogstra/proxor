#include "MacHelperClient.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>

namespace {

using namespace MacHelperWire;

constexpr int kMaxLineBytes = 1024 * 1024; // same cap as the helper's ReadRequest
constexpr int kMaxHelloBudgetMs = 3000;    // connect + hello never waits longer than this

struct WaitScope {
    explicit WaitScope(bool &flag) : flag_(flag), previous_(flag) { flag_ = true; }
    ~WaitScope() { flag_ = previous_; }
    bool &flag_;
    bool previous_;
};

QLatin1String L(const char *s) { return QLatin1String(s); }

} // namespace

MacHelperClient::Paths MacHelperClient::DefaultPaths() {
    return {QString::fromUtf8(kSocketPath), QString::fromUtf8(kPlistPath), QString::fromUtf8(kBinaryPath)};
}

MacHelperClient::MacHelperClient(Paths paths, QObject *parent)
    : QObject(parent), paths_(std::move(paths)), sock_(new QLocalSocket(this)) {
    connect(sock_, &QLocalSocket::readyRead, this, &MacHelperClient::onReadyRead);
    connect(sock_, &QLocalSocket::disconnected, this, &MacHelperClient::noteDisconnected);
}

bool MacHelperClient::isConnected() const {
    return helloDone_ && sock_->state() == QLocalSocket::ConnectedState;
}

void MacHelperClient::drain() {
    const QByteArray data = sock_->readAll();
    if (data.isEmpty()) return;
    gotBytes_ = true;
    buf_.append(data);
}

// Extracts the next complete non-event object. Events are dispatched (queued) on the way; lines that
// are not JSON objects are skipped. Sets overflow_ when a line exceeds the cap.
bool MacHelperClient::takeObject(QJsonObject &obj) {
    for (;;) {
        const int nl = buf_.indexOf('\n');
        if (nl < 0) {
            if (buf_.size() > kMaxLineBytes) overflow_ = true;
            return false;
        }
        if (nl > kMaxLineBytes) {
            overflow_ = true;
            return false;
        }
        const QByteArray line = buf_.left(nl);
        buf_.remove(0, nl + 1);
        if (line.trimmed().isEmpty()) continue;
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) continue;
        obj = doc.object();
        if (obj.contains(L(kKeyEvent))) {
            dispatchEvent(obj);
            continue;
        }
        return true;
    }
}

// Dispatches buffered events and drops replies nobody is waiting for.
void MacHelperClient::processBuffered() {
    QJsonObject ignored;
    while (takeObject(ignored)) {
    }
    if (overflow_) dropConnection();
}

void MacHelperClient::settle() {
    if (sock_->state() == QLocalSocket::ConnectedState) drain();
    processBuffered();
    if (sock_->state() != QLocalSocket::ConnectedState) noteDisconnected();
}

void MacHelperClient::dispatchEvent(const QJsonObject &obj) {
    const QString event = obj.value(L(kKeyEvent)).toString();
    if (event == L(kEventTunReady)) {
        QMetaObject::invokeMethod(this, [this] { emit tunReady(); }, Qt::QueuedConnection);
    } else if (event == L(kEventTunStopped)) {
        const QString reason = obj.value(L(kKeyReason)).toString();
        QMetaObject::invokeMethod(this, [this, reason] { emit tunStopped(reason); }, Qt::QueuedConnection);
    } else if (event == L(kEventLog)) {
        const QString line = obj.value(L(kKeyLine)).toString();
        QMetaObject::invokeMethod(this, [this, line] { emit helperLog(line); }, Qt::QueuedConnection);
    }
}

// Idempotent. Only a session whose hello completed can be "lost"; failures while connecting are
// reported through the probe/reply error instead.
void MacHelperClient::noteDisconnected() {
    if (!helloDone_) return;
    helloDone_ = false;
    buf_.clear();
    QMetaObject::invokeMethod(this, [this] { emit connectionLost(); }, Qt::QueuedConnection);
}

void MacHelperClient::dropConnection() {
    if (sock_->state() != QLocalSocket::UnconnectedState) sock_->abort();
    noteDisconnected();
    buf_.clear();
}

void MacHelperClient::onReadyRead() {
    if (inWait_) return; // a synchronous wait owns the stream and drains it itself
    drain();
    processBuffered();
}

void MacHelperClient::disconnectFromHelper() {
    // Clearing helloDone_ first makes this a deliberate close: no connectionLost.
    helloDone_ = false;
    buf_.clear();
    if (sock_->state() != QLocalSocket::UnconnectedState) sock_->abort();
}

bool MacHelperClient::sendObject(const QJsonObject &obj, int budgetMs) {
    QByteArray line = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    line.append('\n');
    if (sock_->write(line) != line.size()) return false;
    if (sock_->bytesToWrite() > 0) sock_->waitForBytesWritten(qMax(1, budgetMs));
    return true;
}

MacHelperClient::WaitResult MacHelperClient::waitReply(qint64 id, int budgetMs, QJsonObject &reply) {
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        if (sock_->state() == QLocalSocket::ConnectedState) drain();
        QJsonObject obj;
        while (takeObject(obj)) {
            if (obj.contains(L(kKeyId)) && qint64(obj.value(L(kKeyId)).toDouble(-1)) == id) {
                reply = obj;
                processBuffered(); // events queued behind the reply must not wait for the next request
                return WaitResult::Got;
            }
            // reply to an earlier request that timed out: not ours, drop it
        }
        if (overflow_) {
            dropConnection();
            return WaitResult::Overflow;
        }
        if (sock_->state() != QLocalSocket::ConnectedState) return WaitResult::Closed;
        const qint64 remaining = budgetMs - timer.elapsed();
        if (remaining <= 0) return WaitResult::Timeout;
        sock_->waitForReadyRead(int(remaining));
    }
}

// Makes sure there is a live connection with a completed hello, connecting and saying hello if not.
// Fills `p` (connected/helloOk/rejected/protocol/build/singbox/error). True only when the session is
// usable (hello accepted with our protocol version).
bool MacHelperClient::ensureSession(int budgetMs, MacHelperProbe &p) {
    WaitScope scope(inWait_);
    if (helloDone_ && sock_->state() == QLocalSocket::ConnectedState) {
        // Notice a peer that went away (and pick up queued events) before trusting the session.
        sock_->waitForReadyRead(0);
        settle();
    }
    if (helloDone_ && sock_->state() == QLocalSocket::ConnectedState) {
        p.connected = true;
        p.helloOk = true;
        p.protocol = hello_.protocol;
        p.build = hello_.build;
        p.singbox = hello_.singbox;
        return true;
    }

    const int budget = qMax(1, qMin(budgetMs, kMaxHelloBudgetMs));
    helloDone_ = false;
    if (sock_->state() != QLocalSocket::UnconnectedState) sock_->abort();
    buf_.clear();
    overflow_ = false;
    gotBytes_ = false;

    QElapsedTimer timer;
    timer.start();
    sock_->connectToServer(paths_.socket);
    if (!sock_->waitForConnected(budget)) {
        p.error = sock_->errorString();
        sock_->abort();
        return false;
    }
    p.connected = true;

    const qint64 id = nextId_++;
    QJsonObject hello;
    hello.insert(L(kKeyId), double(id));
    hello.insert(L(kKeyCmd), L(kCmdHello));
    hello.insert(L(kKeyProtocol), kMacHelperProtocolVersion);
    QJsonObject reply;
    WaitResult result = WaitResult::Closed;
    if (sendObject(hello, budget)) {
        result = waitReply(id, qMax(1, budget - int(timer.elapsed())), reply);
    }
    switch (result) {
    case WaitResult::Closed:
        // The helper hangs up without a word when the peer uid is not on its allowlist.
        p.rejected = !gotBytes_;
        p.error = p.rejected ? QStringLiteral("connection closed by the helper") : QStringLiteral("connection lost");
        sock_->abort();
        return false;
    case WaitResult::Timeout:
        p.error = QStringLiteral("timeout");
        sock_->abort();
        return false;
    case WaitResult::Overflow:
        p.error = QStringLiteral("reply too long");
        return false;
    case WaitResult::Got:
        break;
    }

    p.protocol = reply.value(L(kKeyProtocol)).toInt(0);
    if (!reply.value(L(kKeyOk)).toBool()) {
        p.error = reply.value(L(kKeyError)).toString();
        if (p.error.isEmpty()) p.error = QStringLiteral("hello rejected");
        sock_->abort();
        return false;
    }
    p.helloOk = true;
    p.build = reply.value(L(kKeyBuild)).toString();
    p.singbox = reply.value(L(kKeySingBox)).toString();
    if (p.protocol != kMacHelperProtocolVersion) {
        p.error = QStringLiteral("protocol mismatch");
        sock_->abort();
        return false;
    }
    hello_ = p;
    helloDone_ = true;
    // The helper may have answered and closed in the same read.
    if (sock_->state() != QLocalSocket::ConnectedState) {
        noteDisconnected();
        return false;
    }
    return true;
}

MacHelperProbe MacHelperClient::probe(int timeoutMs) {
    MacHelperProbe p;
    p.plistPresent = QFileInfo::exists(paths_.plist);
    p.binaryPresent = QFileInfo::exists(paths_.binary);
    ensureSession(timeoutMs, p);
    WaitScope scope(inWait_);
    settle();
    return p;
}

MacHelperState MacHelperClient::state(int timeoutMs) {
    return ClassifyMacHelper(probe(timeoutMs));
}

MacHelperClient::Reply MacHelperClient::request(QJsonObject req, int timeoutMs) {
    Reply out;
    WaitScope scope(inWait_);
    MacHelperProbe p;
    if (!ensureSession(timeoutMs, p)) {
        out.error = p.error.isEmpty() ? QStringLiteral("the Proxor service is not available") : p.error;
        settle();
        return out;
    }

    const qint64 id = nextId_++;
    req.insert(L(kKeyId), double(id));
    if (!sendObject(req, timeoutMs)) {
        settle();
        out.error = QStringLiteral("connection lost");
        return out;
    }
    QJsonObject reply;
    switch (waitReply(id, qMax(1, timeoutMs), reply)) {
    case WaitResult::Got:
        out.body = reply;
        out.ok = reply.value(L(kKeyOk)).toBool();
        out.error = reply.value(L(kKeyError)).toString();
        break;
    case WaitResult::Timeout:
        out.error = QStringLiteral("timeout");
        break;
    case WaitResult::Closed:
        out.error = QStringLiteral("connection lost");
        break;
    case WaitResult::Overflow:
        out.error = QStringLiteral("reply too long");
        break;
    }
    settle();
    return out;
}

MacHelperClient::Reply MacHelperClient::tunStart(const QByteArray &config, int socksPort, int timeoutMs) {
    QJsonObject req;
    req.insert(L(kKeyCmd), L(kCmdTunStart));
    req.insert(L(kKeyConfig), QString::fromUtf8(config));
    req.insert(L(kKeySocksPort), socksPort);
    return request(req, timeoutMs);
}

MacHelperClient::Reply MacHelperClient::tunStop(int timeoutMs) {
    QJsonObject req;
    req.insert(L(kKeyCmd), L(kCmdTunStop));
    return request(req, timeoutMs);
}

MacHelperClient::Reply MacHelperClient::sysproxyApply(int port, const QStringList &bypass, int timeoutMs) {
    QJsonObject req;
    req.insert(L(kKeyCmd), L(kCmdSysproxyApply));
    req.insert(L(kKeyPort), port);
    req.insert(L(kKeyBypass), QJsonArray::fromStringList(bypass));
    return request(req, timeoutMs);
}

MacHelperClient::Reply MacHelperClient::sysproxyRestore(int timeoutMs) {
    QJsonObject req;
    req.insert(L(kKeyCmd), L(kCmdSysproxyRestore));
    return request(req, timeoutMs);
}

MacHelperClient::Reply MacHelperClient::status(int timeoutMs) {
    QJsonObject req;
    req.insert(L(kKeyCmd), L(kCmdStatus));
    return request(req, timeoutMs);
}

MacHelperClient::Reply MacHelperClient::uninstall(int timeoutMs) {
    QJsonObject req;
    req.insert(L(kKeyCmd), L(kCmdUninstall));
    return request(req, timeoutMs);
}

QString MacShellQuote(const QString &value) {
    QString escaped = value;
    escaped.replace(QLatin1Char('\''), QStringLiteral("'\"'\"'"));
    return QLatin1Char('\'') + escaped + QLatin1Char('\'');
}

QString MacAdminAppleScript(const QString &program, const QStringList &args) {
    QStringList parts;
    parts << MacShellQuote(program);
    for (const QString &arg : args) parts << MacShellQuote(arg);
    QString command = parts.join(QLatin1Char(' '));
    // AppleScript string literal: escape backslash first, then the double quote.
    command.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    command.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QStringLiteral("do shell script \"") + command + QStringLiteral("\" with administrator privileges");
}
