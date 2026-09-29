#include "MacHelperService.h"

// RED stubs: the contract compiles, nothing runs yet.

MacHelperService::MacHelperService(MacHelperClient::Paths, QObject *parent) : QObject(parent) {}
MacHelperService::~MacHelperService() = default;

void MacHelperService::probe(QObject *, int, ProbeFn) {}
void MacHelperService::tunStart(QObject *, const QByteArray &, int, int, ReplyFn) {}
void MacHelperService::tunStop(QObject *, int, ReplyFn) {}
void MacHelperService::sysproxyApply(QObject *, int, const QStringList &, int, ReplyFn) {}
void MacHelperService::sysproxyRestore(QObject *, int, ReplyFn) {}
void MacHelperService::status(QObject *, int, ReplyFn) {}
void MacHelperService::uninstall(QObject *, int, ReplyFn) {}
void MacHelperService::disconnectFromHelper() {}

bool MacHelperService::isConnected() const { return false; }
MacHelperState MacHelperService::lastState() const { return MacHelperState::NotInstalled; }
int MacHelperService::pending() const { return 0; }
bool MacHelperService::shutdown(int) { return true; }

bool MacHelperService::enqueue(Job) { return false; }
void MacHelperService::submit(QObject *, Work, ReplyFn) {}
void MacHelperService::deliver(QObject *, std::function<void()>) {}

MacHelperService *MacHelperSvc() {
    static MacHelperService *s = new MacHelperService();
    return s;
}
