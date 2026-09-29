#pragma once

// One-time admin installer for the macOS privileged helper. Widgets are allowed here (it shows the
// explanation dialog); the protocol client itself stays Widgets-free. Compiled on APPLE only.

#include <QString>
#include <functional>

#include "MacHelperPolicy.h"

class QWidget;

namespace MacHelperInstaller {

// QDir(applicationDirPath() + "/../Resources/helper").filePath(name), canonical; empty when missing.
QString BundledScriptPath(const QString &name);

// Explains, then (if the user agrees) runs helper-install.sh through one admin prompt. Never blocks
// the UI thread on the password dialog: `done` is invoked later, exactly once.
// `feature` is tr("Tun Mode") or tr("System Proxy"); `action` comes from DecideMacHelperEnable.
void ConfirmAndInstall(QWidget *parent, const QString &feature, MacHelperEnableAction action,
                       std::function<void(MacAdminScriptResult)> done);

// True from the moment the explanation dialog opens until the install finished (or was declined).
// Callers must not open a second installer while it is true. UI thread only.
bool InstallInProgress();

// Removal: uses the helper's own `uninstall` command when it answers (no password), otherwise runs
// helper-uninstall.sh through one admin prompt.
void Uninstall(QWidget *parent, std::function<void(MacAdminScriptResult)> done);

} // namespace MacHelperInstaller
