#pragma once

// Asynchronous facade over MacHelperClient (Qt Core + Network only: no Widgets, no macOS APIs).
// The synchronous client lives on a dedicated worker thread, so no call made through this class ever
// blocks the caller (the GUI thread); results come back through callbacks.

#include <QByteArray>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QStringList>

#include <atomic>
#include <functional>

#include "MacHelperClient.h"

class QThread;

// Asynchronous facade over MacHelperClient: the client runs on its own worker thread, so no call ever
// blocks the caller. Results come back on the thread that owns `context`, exactly once, in call
// order, unless `context` was destroyed first (then the callback is dropped). A null `context`
// means fire and forget (no callback).
class MacHelperService : public QObject {
    Q_OBJECT
public:
    using Reply = MacHelperClient::Reply;
    using ReplyFn = std::function<void(const MacHelperService::Reply &)>;
    using ProbeFn = std::function<void(const MacHelperProbe &, MacHelperState)>;

    explicit MacHelperService(MacHelperClient::Paths paths = MacHelperClient::DefaultPaths(), QObject *parent = nullptr);
    ~MacHelperService() override; // shutdown(30000): waits for the in-flight request (bounded by its timeout)

    void probe(QObject *context, int timeoutMs, ProbeFn done);
    void tunStart(QObject *context, const QByteArray &config, int socksPort, int timeoutMs, ReplyFn done);
    void tunStop(QObject *context, int timeoutMs, ReplyFn done);
    void sysproxyApply(QObject *context, int port, const QStringList &bypass, int timeoutMs, ReplyFn done);
    void sysproxyRestore(QObject *context, int timeoutMs, ReplyFn done);
    void status(QObject *context, int timeoutMs, ReplyFn done);
    void uninstall(QObject *context, int timeoutMs, ReplyFn done);
    void disconnectFromHelper(); // queued; deliberate close, no connectionLost

    bool isConnected() const;         // cached, never blocks
    MacHelperState lastState() const; // cached classification of the last probe/request (NotInstalled initially)
    int pending() const;              // queued + running requests
    // Exit path: deliberate disconnect (the helper's lease cleanup stops Tun and restores the proxy),
    // then quit the worker and wait at most waitMs. Idempotent. Returns true if the worker stopped.
    bool shutdown(int waitMs);

signals:
    void tunReady();
    void tunStopped(const QString &reason);
    void helperLog(const QString &line);
    void connectionLost();

private:
    using Job = std::function<void(MacHelperClient &)>;
    using Work = std::function<Reply(MacHelperClient &)>;
    // Posts `job` to the worker thread; false (nothing posted) once shutdown() ran.
    bool enqueue(Job job);
    void submit(QObject *context, Work work, ReplyFn done);
    static void deliver(QObject *context, std::function<void()> fn);

    QThread *worker_ = nullptr;
    MacHelperClient *client_ = nullptr; // lives on worker_
    std::atomic<bool> connected_{false};
    std::atomic<int> state_{int(MacHelperState::NotInstalled)};
    std::atomic<int> pending_{0};
    bool down_ = false; // guarded by mutex_ (submit and shutdown post under it, so FIFO holds)
    QMutex mutex_;
};

// App singleton, created on first use on the UI thread and intentionally never destroyed (a QThread
// must not be destroyed while running; the exit path calls shutdown()). UI thread only.
MacHelperService *MacHelperSvc();
