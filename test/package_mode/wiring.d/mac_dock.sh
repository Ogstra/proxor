#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
route=R2   # MAC-DOCK decision in 53-RESEARCH.md
fail() { echo "mac_dock.sh: $*" >&2; exit 1; }
mm="$repo_root/src/ui/mac/MacPlatform.mm"
mw="$repo_root/src/ui/mainwindow.cpp"

case "$route" in
  R2)
    grep -q 'InstallReopenHandler' "$mm" || fail "MacPlatform.mm must define InstallReopenHandler"
    grep -q 'kAEReopenApplication' "$mm" || fail "MacPlatform.mm must register for kAEReopenApplication"
    awk '/^#ifdef Q_OS_MACOS/{m=1} /^#(else|endif)/{m=0} m && /ProxorMac::InstallReopenHandler\(/{f=1} END{exit !f}' "$mw" \
      || fail "mainwindow.cpp must call ProxorMac::InstallReopenHandler( inside #ifdef Q_OS_MACOS"
    grep -q 'DecideReopen(' "$mw" || fail "mainwindow.cpp must use DecideReopen"
    ;;
  R1)
    grep -q 'RepeatActiveDetector' "$mm" || fail "MacPlatform.mm must use RepeatActiveDetector"
    grep -q 'InstallReopenHandler(' "$mw" || fail "mainwindow.cpp must call InstallReopenHandler"
    ;;
  R3)
    grep -q 'Clicking the Dock icon does not reopen it yet' "$mw" || fail "mainwindow.cpp must explain the Dock limitation"
    ;;
esac
if grep -n 'Q_OS_' "$repo_root"/src/platform/MacReopenPolicy.*; then fail "policy must be OS-free"; fi
grep -q 'src/ui/mac/MacPlatform.mm' "$repo_root/cmake/macos/macos.cmake" || fail "macos.cmake must list MacPlatform.mm"
for f in cmake/windows/windows.cmake cmake/linux/linux.cmake CMakeLists.txt; do
  [ -f "$repo_root/$f" ] || continue
  if grep -q 'MacPlatform' "$repo_root/$f"; then fail "MacPlatform must be named only by macos.cmake ($f)"; fi
done
echo "mac_dock: OK"
