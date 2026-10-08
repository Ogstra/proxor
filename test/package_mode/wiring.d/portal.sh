#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "portal.sh: $1" >&2; exit 1; }

lin="$repo_root/cmake/linux/linux.cmake"
win="$repo_root/cmake/windows/windows.cmake"
mac="$repo_root/cmake/macos/macos.cmake"
sys="$repo_root/src/sys"

for f in XdgPortal.cpp XdgPortal.hpp DesktopPortalLinux.cpp PortalBackground.cpp PortalScreenshot.cpp PortalGlobalShortcuts.cpp; do
  grep -qF "src/sys/linux/$f" "$lin" || fail "linux.cmake must list $f"
done
if grep -qF 'DesktopPortalNone.cpp' "$lin"; then fail "linux.cmake must not list DesktopPortalNone.cpp"; fi
for os in windows macos; do
  f="$repo_root/cmake/$os/$os.cmake"
  grep -qF 'DesktopPortalNone.cpp' "$f" || fail "$os.cmake must list DesktopPortalNone.cpp"
  if grep -qE 'XdgPortal|PortalBackground|PortalScreenshot|PortalGlobalShortcuts|DesktopPortalLinux|DBus' "$f"; then
    fail "$os.cmake must not mention the Linux portal files or DBus"
  fi
done

# Exactly one definition of each facade function per OS list.
count_defs() { # pattern file...
  local pat="$1"; shift
  local n=0 f
  for f in "$@"; do
    if grep -qE "$pat" "$f"; then n=$((n + 1)); fi
  done
  echo "$n"
}
linux_files=("$sys/linux/DesktopPortalLinux.cpp" "$sys/linux/PortalBackground.cpp" "$sys/linux/PortalScreenshot.cpp" "$sys/linux/PortalGlobalShortcuts.cpp" "$sys/linux/XdgPortal.cpp")
none_files=("$sys/DesktopPortalNone.cpp")
for pat in '^void RequestAutostart\(bool' '^void TakeScreenshot\(' 'CreateGlobalShortcutSession\(QObject' '^void StartPortalProbe\(\)' '^const PortalVersions &Portals\(\)'; do
  [ "$(count_defs "$pat" "${linux_files[@]}")" = 1 ] || fail "Linux list must define '$pat' exactly once"
  [ "$(count_defs "$pat" "${none_files[@]}")" = 1 ] || fail "None list must define '$pat' exactly once"
done
grep -qE '^void RequestAutostart\(bool' "$sys/linux/PortalBackground.cpp" || fail "RequestAutostart belongs in PortalBackground.cpp"
grep -qE '^void TakeScreenshot\(' "$sys/linux/PortalScreenshot.cpp" || fail "TakeScreenshot belongs in PortalScreenshot.cpp"
grep -qE 'CreateGlobalShortcutSession\(QObject' "$sys/linux/PortalGlobalShortcuts.cpp" || fail "CreateGlobalShortcutSession belongs in PortalGlobalShortcuts.cpp"
grep -qF 'StartPortalProbe' "$sys/DesktopPortal.hpp" || fail "facade lost StartPortalProbe"

if grep -n 'Q_OS_' "$sys/DesktopPortal.hpp" "$sys/DesktopPortalNone.cpp" "$sys"/linux/XdgPortal.* "$sys/linux/DesktopPortalLinux.cpp" "$sys"/linux/Portal*.cpp >&2; then
  fail "no Q_OS_ in the portal sources"
fi
if grep -qE 'ProxorGui|include "ui/|QtWidgets|QWidget' "$sys"/linux/XdgPortal.*; then fail "XdgPortal must not include GUI code"; fi
grep -qF 'std::thread' "$sys/linux/DesktopPortalLinux.cpp" || fail "probe must run on a worker thread"
grep -qF 'wait_for(' "$sys/linux/DesktopPortalLinux.cpp" || fail "Portals() must wait with a bound"
echo "portal: OK"
