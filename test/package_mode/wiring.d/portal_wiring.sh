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
  added=$(diff <(git -C "$repo_root" show "$base:$rel" | grep -c 'Q_OS_' || true) <(grep -c 'Q_OS_' "$repo_root/$rel" || true) | grep -E '^>' || true)
  [ -z "$added" ] || fail "mainwindow.cpp gained Q_OS_ lines versus the phase base"
  w=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX) <(unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX "$repo_root/$rel") | grep -E '^[<>]' | sort || true)
  m=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX) <(unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX "$repo_root/$rel") | grep -E '^[<>]' | sort || true)
  [ "$w" = "$m" ] || fail "Windows and macOS views of $rel changed differently"
fi
echo "portal_wiring: OK"
