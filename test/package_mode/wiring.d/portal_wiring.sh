#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "portal_wiring.sh: $*" >&2; exit 1; }
mw="$repo_root/src/ui/mainwindow.cpp"
mainf="$repo_root/src/main/main.cpp"

grep -qF 'StartPortalProbe()' "$mainf" || fail "main.cpp must start the portal probe"
probe_line=$(grep -nF 'StartPortalProbe()' "$mainf" | head -1 | cut -d: -f1)
ui_line=$(grep -nE 'UI_InitMainWindow\(|new MainWindow|MainWindow [a-z]+;' "$mainf" | head -1 | cut -d: -f1 || true)
if [ -n "$ui_line" ] && [ "$probe_line" -ge "$ui_line" ]; then fail "StartPortalProbe() must run before the main window is built"; fi

for needle in 'HotkeyBackend::Portal' 'CreateGlobalShortcutSession(' 'PortalTriggerFromKeySequence(' \
              'PortalHotkeySession.reset()' 'ScreenCaptureBackend::Portal' 'ProxorDesktop::TakeScreenshot(' \
              'std::make_shared<QHotkey>' 'grabWindow(0'; do
  grep -qF "$needle" "$mw" || fail "mainwindow.cpp must contain $needle"
done
grep -qF 'Portals()' "$repo_root/src/platform/PlatformCapabilitiesApp.cpp" || fail "PlatformCapabilitiesApp.cpp must read Portals()"
grep -qF 'ShouldRequestFlatpakAutostart(' "$repo_root/src/sys/AutoRun.cpp" || fail "AutoRun.cpp must not re-request an unchanged state"

base="$(cd "$repo_root" && sh .planning/phases/52-linux-desktop-integration/phase-base.sh 2>/dev/null || true)"
if [ -n "$base" ] && command -v unifdef >/dev/null; then
  rel=src/ui/mainwindow.cpp
  # Phase 53 adds Q_OS_MACOS blocks on purpose (Windows/Linux views are proven by unifdef in 53-08): count the other Q_OS_ lines.
  added=$(diff <(git -C "$repo_root" show "$base:$rel" | grep 'Q_OS_' | grep -vc 'Q_OS_MACOS' || true) <(grep 'Q_OS_' "$repo_root/$rel" | grep -vc 'Q_OS_MACOS' || true) | grep -E '^>' || true)
  [ -z "$added" ] || fail "mainwindow.cpp gained Q_OS_ lines versus the phase base"
  w=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX) <(unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX "$repo_root/$rel") | grep -E '^[<>]' | sort || true)
  # Phase 53 adds macOS-only hunks on purpose: judge the macOS view as of the phase-53 base.
  mac_head="$(cd "$repo_root" && sh .planning/phases/53-macos-desktop-integration/phase-base.sh 2>/dev/null || true)"
  [ -n "$mac_head" ] || mac_head="HEAD"
  m=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX) <(git -C "$repo_root" show "$mac_head:$rel" | unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX) | grep -E '^[<>]' | sort || true)
  [ "$w" = "$m" ] || fail "Windows and macOS views of $rel changed differently"
fi
echo "portal_wiring: OK"
