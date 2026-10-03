#pragma once
// org.freedesktop.portal.GlobalShortcuts session. QtCore + QtDBus only.
#include "sys/DesktopPortal.hpp"
#include "sys/linux/XdgPortal.hpp"

#include <QDBusArgument>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariantMap>

namespace ProxorDesktop {

// D-Bus (sa{sv}); QList<PortalShortcut> is a(sa{sv}).
struct PortalShortcut {
    QString id;
    QVariantMap options;
};
QDBusArgument &operator<<(QDBusArgument &arg, const PortalShortcut &s);
const QDBusArgument &operator>>(const QDBusArgument &arg, PortalShortcut &s);
void RegisterPortalShortcutTypes(); // idempotent

std::unique_ptr<GlobalShortcutSession> CreateGlobalShortcutSessionWith(const XdgPortalClient &client, uint version, QObject *context,
                                                                       int timeoutMs = 120000);

} // namespace ProxorDesktop

Q_DECLARE_METATYPE(ProxorDesktop::PortalShortcut)
Q_DECLARE_METATYPE(QList<ProxorDesktop::PortalShortcut>)
