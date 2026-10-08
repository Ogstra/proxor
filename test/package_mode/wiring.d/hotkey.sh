#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
mw="$repo_root/src/ui/mainwindow.cpp"
dh="$repo_root/src/ui/dialog_hotkey.cpp"
for pat in 'ProxorPlatform::PlanHotkeyRegistration(' 'Capability::GlobalHotkeys' 'HotkeyRejectedText' 'RegisteredHotkey.clear()'; do
  grep -qF "$pat" "$mw" || { echo "hotkey.sh: mainwindow.cpp missing $pat" >&2; exit 1; }
done
grep -qF 'QStringList RegisterHotkey(bool unregister);' "$repo_root/src/ui/mainwindow.h" \
  || { echo "hotkey.sh: mainwindow.h must declare QStringList RegisterHotkey(bool unregister);" >&2; exit 1; }
for pat in 'hotkey_conflict_note' 'hotkey_capability_note' 'ApplyCapability('; do
  grep -qF "$pat" "$dh" || { echo "hotkey.sh: dialog_hotkey.cpp missing $pat" >&2; exit 1; }
done
if awk '/^QStringList MainWindow::RegisterHotkey/,/^}/' "$mw" | grep -q deleteLater; then
  echo "hotkey.sh: RegisterHotkey must not deleteLater shared_ptr-owned QHotkey (double delete)" >&2; exit 1
fi
if grep -q 'Conflict hotkey' "$mw"; then
  echo "hotkey.sh: the silent conflict return is back" >&2; exit 1
fi
