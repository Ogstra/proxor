#pragma once
// org.freedesktop.portal.Screenshot, testable with an explicit client and portal version.
#include "sys/DesktopPortal.hpp"
#include "sys/linux/XdgPortal.hpp"

#include <functional>

namespace ProxorDesktop {

void TakeScreenshotWith(const XdgPortalClient &client, uint version, const QString &parentWindow, QObject *context,
                        std::function<void(const ScreenshotResult &)> done, int timeoutMs = 60000);

// tempDirs: QStandardPaths::TempLocation and $XDG_RUNTIME_DIR in production; a path under "<dir>/doc/" is never temporary.
bool IsTemporaryScreenshotPath(const QString &path, const QStringList &tempDirs);

} // namespace ProxorDesktop
