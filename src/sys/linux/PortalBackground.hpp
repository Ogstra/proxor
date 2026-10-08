#pragma once
// Background portal (autostart). QtCore + QtDBus only.
#include "sys/DesktopPortal.hpp"
#include "sys/linux/XdgPortal.hpp"

namespace ProxorDesktop {
// Testable core: version = Background portal version (0 => NotAvailable, no D-Bus call).
// timeoutMs default 120000 (the desktop may show a dialog).
void RequestAutostartWith(const XdgPortalClient &client, uint version, bool enable, const QStringList &commandline,
                          const QString &reason, QObject *context, std::function<void(const AutostartResult &)> done,
                          int timeoutMs = 120000);
} // namespace ProxorDesktop
