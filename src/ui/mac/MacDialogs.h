// src/ui/mac/MacDialogs.h — runtime macOS look for Proxor's dialogs. Plain Qt (no Objective-C).
// Include only under #ifdef Q_OS_MACOS.
#pragma once

namespace ProxorMac {
// Installs an application-wide filter that polishes Proxor's dialogs (classes named Dialog*) when
// they are first shown: centered over their parent, label columns aligned, rounded line edits,
// natural-size buttons, compact group boxes. The .ui files stay untouched (they are shared with
// Windows and Linux); everything is adjusted at runtime, by object name. Call once.
void InstallDialogPolish();
}
