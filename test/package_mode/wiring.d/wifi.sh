#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "wifi.sh: $1" >&2; exit 1; }

# No process on the UI thread: the monitor itself never spawns or waits for one.
for f in "$repo_root/src/sys/WifiMonitor.cpp" "$repo_root/src/sys/WifiMonitor.hpp"; do
  if grep -qE 'QProcess|waitForFinished' "$f"; then fail "$f must not run a process"; fi
done

mw="$repo_root/src/ui/mainwindow.cpp"
grep -qF 'CreatePlatformWifiBackend()' "$mw" || fail "mainwindow.cpp must create the monitor from CreatePlatformWifiBackend()"
grep -qF 'WifiMonitor::setAppInstance(' "$mw" || fail "mainwindow.cpp must register the app monitor"
grep -qF 'WifiMonitor::cachedSsid()' "$repo_root/src/db/ConfigBuilder.cpp" || fail "ConfigBuilder.cpp lost WifiMonitor::cachedSsid()"

# Each OS compiles exactly one backend and one permission implementation, chosen by its cmake file.
for os in windows linux macos; do
  list="$repo_root/cmake/$os/$os.cmake"
  files=$(grep -oE 'src/sys/wifi/[A-Za-z]+\.(cpp|mm)' "$list" | sort -u || true)
  [ -n "$files" ] || fail "$list lists no src/sys/wifi source"
  backends=0
  perms=0
  for rel in $files; do
    src="$repo_root/$rel"
    [ -f "$src" ] || fail "$list names missing file $rel"
    backends=$((backends + $(grep -cE '^std::unique_ptr<WifiBackend> CreatePlatformWifiBackend\(\)' "$src" || true)))
    perms=$((perms + $(grep -cE '^(ProxorWifi::)?PermissionState (ProxorWifi::)?CurrentWifiPermission\(\)' "$src" || true)))
  done
  [ "$backends" -eq 1 ] || fail "$os must define CreatePlatformWifiBackend() exactly once (found $backends)"
  [ "$perms" -eq 1 ] || fail "$os must define CurrentWifiPermission() exactly once (found $perms)"
done

if grep -rn 'Q_OS_' "$repo_root/src/sys/wifi" "$repo_root"/src/sys/WifiMonitor.* "$repo_root"/src/platform/WifiSsid.* >&2; then
  fail "no Q_OS_ conditionals in the Wi-Fi sources"
fi
echo "wifi: OK"
