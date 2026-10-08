#include "MacHelperService.h"

#include <QDebug>
#include <QMetaObject>
#include <QMutexLocker>
#include <QThread>

namespace {

constexpr int kShutdownWaitMs = 30000;

MacHelperClient::Reply ShutDownReply() {
    MacHelperClient::Reply r;
    r.ok = false;
    r.error = QStringLiteral("the Proxor service client is shut down");
    return r;
}

} // namespace

MacHelperService::MacHelperService(MacHelperClient::Paths paths, QObject *parent)
    : QObject(parent), worker_(new QThread), client_(new MacHelperClient(std::move(paths))) {
    worker_->setObjectName(QStringLiteral("MacHelperService"));
    // The client (and its QLocalSocket child) is created here but only ever used on the worker:
    // move it before the thread starts so no other thread touches it afterwards.
    client_->moveToThread(worker_);

    // Cache invalidation runs on the worker (the client emits there), before the forwarded signal.
    connect(client_, &MacHelperClient::connectionLost, client_, [this] { connected_ = false; });
    connect(client_, &MacHelperClient::tunReady, this, &MacHelperService::tunReady, Qt::QueuedConnection);
    connect(client_, &MacHelperClient::tunStopped, this, &MacHelperService::tunStopped, Qt::QueuedConnection);
    connect(client_, &MacHelperClient::helperLog, this, &MacHelperService::helperLog, Qt::QueuedConnection);
    connect(client_, &MacHelperClient::connectionLost, this, &MacHelperService::connectionLost,
            Qt::QueuedConnection);
    connect(worker_, &QThread::finished, client_, &QObject::deleteLater);
    worker_->start();
}

MacHelperService::~MacHelperService() {
    shutdown(kShutdownWaitMs);
    if (worker_->isRunning()) {
        // A QThread must never be destroyed while running (Qt aborts): keep waiting.
        qWarning() << "MacHelperService: worker still busy after" << kShutdownWaitMs << "ms, waiting for it";
        worker_->wait();
    }
    delete worker_;
}

bool MacHelperService::enqueue(Job job) {
    QMutexLocker lock(&mutex_);
    if (down_) return false;
    ++pending_;
    QMetaObject::invokeMethod(client_, [this, job = std::move(job)] { job(*client_); }, Qt::QueuedConnection);
    return true;
}

// Called from the worker thread with a possibly-dying context: the queued functor is dropped by Qt
// if the context is destroyed before it runs, and the callers re-check their QPointer inside.
void MacHelperService::deliver(QObject *context, std::function<void()> fn) {
    if (!context) return;
    QMetaObject::invokeMethod(context, std::move(fn), Qt::QueuedConnection);
}

void MacHelperService::submit(QObject *context, Work work, ReplyFn done) {
    QPointer<QObject> ctx(context);
    const bool wantsReply = context && done;
    const bool posted = enqueue([this, ctx, work = std::move(work), done, wantsReply](MacHelperClient &c) {
        const Reply r = work(c);
        connected_ = c.isConnected();
        if (r.ok || connected_) state_ = int(MacHelperState::Ready);
        --pending_;
        if (wantsReply && ctx) deliver(ctx.data(), [ctx, done, r] {
            if (ctx) done(r);
        });
    });
    if (!posted && wantsReply) {
        deliver(context, [ctx, done] {
            if (ctx) done(ShutDownReply());
        });
    }
}

void MacHelperService::probe(QObject *context, int timeoutMs, ProbeFn done) {
    QPointer<QObject> ctx(context);
    const bool wantsReply = context && done;
    const bool posted = enqueue([this, ctx, timeoutMs, done, wantsReply](MacHelperClient &c) {
        const MacHelperProbe p = c.probe(timeoutMs);
        const MacHelperState s = ClassifyMacHelper(p);
        connected_ = c.isConnected();
        state_ = int(s);
        --pending_;
        if (wantsReply && ctx) deliver(ctx.data(), [ctx, done, p, s] {
            if (ctx) done(p, s);
        });
    });
    if (!posted && wantsReply) {
        MacHelperProbe p;
        p.error = ShutDownReply().error;
        deliver(context, [ctx, done, p] {
            if (ctx) done(p, MacHelperState::NotInstalled);
        });
    }
}

void MacHelperService::tunStart(QObject *context, const QByteArray &config, int socksPort, int timeoutMs,
                                ReplyFn done) {
    submit(context, [config, socksPort, timeoutMs](MacHelperClient &c) { return c.tunStart(config, socksPort, timeoutMs); },
           std::move(done));
}

void MacHelperService::tunStop(QObject *context, int timeoutMs, ReplyFn done) {
    submit(context, [timeoutMs](MacHelperClient &c) { return c.tunStop(timeoutMs); }, std::move(done));
}

void MacHelperService::sysproxyApply(QObject *context, int port, const QStringList &bypass, int timeoutMs,
                                     ReplyFn done) {
    submit(context, [port, bypass, timeoutMs](MacHelperClient &c) { return c.sysproxyApply(port, bypass, timeoutMs); },
           std::move(done));
}

void MacHelperService::sysproxyRestore(QObject *context, int timeoutMs, ReplyFn done) {
    submit(context, [timeoutMs](MacHelperClient &c) { return c.sysproxyRestore(timeoutMs); }, std::move(done));
}

void MacHelperService::status(QObject *context, int timeoutMs, ReplyFn done) {
    submit(context, [timeoutMs](MacHelperClient &c) { return c.status(timeoutMs); }, std::move(done));
}

void MacHelperService::uninstall(QObject *context, int timeoutMs, ReplyFn done) {
    submit(context, [timeoutMs](MacHelperClient &c) { return c.uninstall(timeoutMs); }, std::move(done));
}

void MacHelperService::disconnectFromHelper() {
    enqueue([this](MacHelperClient &c) {
        c.disconnectFromHelper();
        connected_ = false;
        --pending_;
    });
}

bool MacHelperService::isConnected() const { return connected_; }

MacHelperState MacHelperService::lastState() const { return MacHelperState(state_.load()); }

int MacHelperService::pending() const { return pending_; }

bool MacHelperService::shutdown(int waitMs) {
    {
        QMutexLocker lock(&mutex_);
        if (!down_) {
            down_ = true;
            // ONE lambda that disconnects and then quits from inside the worker: it runs after every
            // request queued before it, and the lease is closed deliberately (not by socket teardown).
            QMetaObject::invokeMethod(
                client_,
                [this] {
                    client_->disconnectFromHelper();
                    connected_ = false;
                    QThread::currentThread()->quit();
                },
                Qt::QueuedConnection);
        }
    }
    return worker_->wait(waitMs);
}

// Never destroyed on purpose: the worker QThread must not be destroyed while running, and the exit path
// calls shutdown() explicitly.
MacHelperService *MacHelperSvc() {
    static MacHelperService *s = new MacHelperService();
    return s;
}
