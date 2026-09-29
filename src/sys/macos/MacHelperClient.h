#pragma once

// GUI side of the macOS privileged helper protocol (JSON lines over a unix socket).
// Qt Core + Network only: no Widgets, no macOS APIs, so the protocol handling is unit-tested
// against a fake server on every CI runner even though it is only linked into the app on macOS.

#include <QByteArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QStringList>

#include "MacHelperPolicy.h"

// Wire names mirrored from go/cmd/proxor_core/machelper/protocol.go. The client code uses ONLY these
// constants (no inline literals for commands/events/keys) so that a drift check can compare this set
// with the Go constants and json tags.
namespace MacHelperWire {
constexpr const char *kSocketPath = "/var/run/io.github.Ogstra.Proxor.helper.sock";
constexpr const char *kPlistPath = "/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist";
constexpr const char *kBinaryPath = "/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper";
// commands
constexpr const char *kCmdHello = "hello";
constexpr const char *kCmdStatus = "status";
constexpr const char *kCmdTunStart = "tun_start";
constexpr const char *kCmdTunStop = "tun_stop";
constexpr const char *kCmdSysproxyApply = "sysproxy_apply";
constexpr const char *kCmdSysproxyRestore = "sysproxy_restore";
constexpr const char *kCmdUninstall = "uninstall";
// events
constexpr const char *kEventTunReady = "tun_ready";
constexpr const char *kEventTunStopped = "tun_stopped";
constexpr const char *kEventLog = "log";
// json keys (Request, Response, Event tags)
constexpr const char *kKeyId = "id";
constexpr const char *kKeyCmd = "cmd";
constexpr const char *kKeyProtocol = "protocol";
constexpr const char *kKeyConfig = "config";
constexpr const char *kKeySocksPort = "socksPort";
constexpr const char *kKeyPort = "port";
constexpr const char *kKeyBypass = "bypass";
constexpr const char *kKeyOk = "ok";
constexpr const char *kKeyError = "error";
constexpr const char *kKeyBuild = "build";
constexpr const char *kKeySingBox = "singbox";
constexpr const char *kKeyUid = "uid";
constexpr const char *kKeyTunRunning = "tunRunning";
constexpr const char *kKeyProxyApplied = "proxyApplied";
constexpr const char *kKeyApplied = "applied";
constexpr const char *kKeyFailed = "failed";
constexpr const char *kKeyEvent = "event";
constexpr const char *kKeyLine = "line";
constexpr const char *kKeyReason = "reason";
}

// One persistent connection to the helper. That connection is the lease the helper uses for cleanup:
// when it drops (GUI exit or crash) the helper stops Tun and restores the system proxy.
// Requests are synchronous (bounded waits, no nested event loop); events and log lines are emitted
// as queued signals so they are never re-entrant inside request().
class MacHelperClient : public QObject {
    Q_OBJECT
public:
    struct Paths {
        QString socket;
        QString plist;
        QString binary;
    };
    static Paths DefaultPaths();

    explicit MacHelperClient(Paths paths = DefaultPaths(), QObject *parent = nullptr);

    struct Reply {
        bool ok = false;
        QString error;
        QJsonObject body;
    };

    // File presence + connect + hello, never prompts. On success the connection is kept.
    MacHelperProbe probe(int timeoutMs = 1000);
    MacHelperState state(int timeoutMs = 1000); // ClassifyMacHelper(probe(timeoutMs))
    bool isConnected() const;

    // Ensures a connection and a completed hello first; assigns "id".
    Reply request(QJsonObject req, int timeoutMs);
    Reply tunStart(const QByteArray &config, int socksPort, int timeoutMs = 10000);
    Reply tunStop(int timeoutMs = 5000);
    Reply sysproxyApply(int port, const QStringList &bypass, int timeoutMs = 20000);
    Reply sysproxyRestore(int timeoutMs = 20000);
    Reply status(int timeoutMs = 2000);
    Reply uninstall(int timeoutMs = 10000);
    void disconnectFromHelper();

signals:
    void tunReady();
    void tunStopped(const QString &reason);
    void helperLog(const QString &line);
    void connectionLost();

private:
    enum class WaitResult { Got, Timeout, Closed, Overflow };

    bool ensureSession(int budgetMs, MacHelperProbe &probe);
    WaitResult waitReply(qint64 id, int budgetMs, QJsonObject &reply);
    bool sendObject(const QJsonObject &obj, int budgetMs);
    void drain();
    bool takeObject(QJsonObject &obj);
    void processBuffered();
    void settle();
    void dispatchEvent(const QJsonObject &obj);
    void noteDisconnected();
    void dropConnection();
    void onReadyRead();

    Paths paths_;
    QLocalSocket *sock_;
    QByteArray buf_;
    qint64 nextId_ = 1;
    bool helloDone_ = false;
    bool inWait_ = false;
    bool gotBytes_ = false;
    bool overflow_ = false;
    MacHelperProbe hello_; // protocol/build/singbox of the live session
};

QString MacShellQuote(const QString &value);
// "do shell script <quoted command> <admin privileges clause>" for `osascript -e`.
QString MacAdminAppleScript(const QString &program, const QStringList &args);
