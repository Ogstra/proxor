#pragma once
// Scriptable fake of org.freedesktop.portal.Desktop on its own thread and its own D-Bus connection.
#include <QDBusMessage>
#include <QList>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <functional>
#include <memory>

struct FakePortalCall {
    QString path, interface, member, sender;
    QVariantList args;
};

struct FakePortalAnswer { // for request-style methods (last arg = options a{sv})
    QString errorName;    // non-empty: reply with this D-Bus error instead
    uint response = 0;
    QVariantMap results;
    int delayMs = 0;
    bool emitResponse = true;
    QString overrideRequestPath;
};

class FakePortal {
public:
    FakePortal();
    ~FakePortal();
    bool start(); // registerService + registerVirtualObject(/org/freedesktop/portal/desktop, SubPath)
    void setVersion(const QString &iface, uint version);
    void setRequestHandler(const QString &iface, const QString &member, std::function<FakePortalAnswer(const FakePortalCall &)> h);
    void setDirectHandler(const QString &iface, const QString &member, std::function<QDBusMessage(const QDBusMessage &call)> h);
    void emitSignal(const QString &path, const QString &iface, const QString &member, const QVariantList &args);
    QList<FakePortalCall> calls() const; // thread-safe snapshot, in arrival order

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
