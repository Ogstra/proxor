#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "tray.sh: $*" >&2; exit 1; }
mw="$repo_root/src/ui/mainwindow.cpp"

grep -q 'DecideCloseAction(' "$mw" || fail "closeEvent must call DecideCloseAction"
grep -q 'ApplyStartupVisibility(this, 0)' "$mw" || fail "constructor must call ApplyStartupVisibility(this, 0)"
if grep -qF 'if (!ProxorGui::dataStore->flag_tray) show();' "$mw"; then
  fail "the unconditional start-hidden line must be gone"
fi
grep -q 'isSystemTrayAvailable()' "$repo_root/src/platform/PlatformCapabilitiesApp.cpp" || fail "glue must read isSystemTrayAvailable()"
grep -q 'DetectLinuxDesktop(' "$repo_root/src/platform/PlatformCapabilitiesApp.cpp" || fail "glue must detect the Linux desktop"
grep -q 'Capability::SystemTray' "$repo_root/src/ui/dialog_basic_settings.cpp" || fail "Start minimized must be gated on SystemTray"
if grep -nE 'ProxorDesktop::Portals|Portals\(\)' "$mw"; then
  fail "no portal calls on the UI thread here"
fi

base="$(cd "$repo_root" && sh .planning/phases/52-linux-desktop-integration/phase-base.sh 2>/dev/null || true)"
if [ -n "$base" ] && command -v unifdef >/dev/null; then
  rel=src/ui/mainwindow.cpp
  w=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX) <(unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX "$repo_root/$rel") | grep -E '^[<>]' | sort || true)
  # Phase 53 adds macOS-only hunks on purpose: judge the macOS view as of the phase-53 base.
  mac_head="$(cd "$repo_root" && sh .planning/phases/53-macos-desktop-integration/phase-base.sh 2>/dev/null || true)"
  [ -n "$mac_head" ] || mac_head="HEAD"
  m=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX) <(git -C "$repo_root" show "$mac_head:$rel" | unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX) | grep -E '^[<>]' | sort || true)
  [ "$w" = "$m" ] || fail "Windows and macOS views of $rel changed differently"
fi
echo "tray: OK"
