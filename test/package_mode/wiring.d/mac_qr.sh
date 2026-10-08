#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
mw="$repo_root/src/ui/mainwindow.cpp"
mm="$repo_root/src/sys/macos/MacScreenCapture.mm"
pol="$repo_root/src/platform/QrScanPolicy.cpp"
fail() { echo "mac_qr.sh: $1" >&2; exit 1; }
# Both macOS calls must sit inside #ifdef Q_OS_MACOS ranges of mainwindow.cpp.
for pat in 'macScreenCaptureReadyOrExplain()' 'DecideMacScreenScan('; do
  n=$(awk '/^#ifdef Q_OS_MACOS/{m=1} m&&index($0,p){c++} /^#(else|endif)/{m=0} END{print c+0}' p="$pat" "$mw")
  [ "$n" -ge 1 ] || fail "mainwindow.cpp: $pat not inside an #ifdef Q_OS_MACOS range"
done
grep -qF 'const auto cap = CurrentCapability(Capability::ScreenQrCapture);' "$mw" || fail "original capability line is gone"
grep -q 'CGPreflightScreenCaptureAccess' "$mm" || fail "MacScreenCapture.mm lacks preflight"
grep -q 'CGRequestScreenCaptureAccess' "$mm" || fail "MacScreenCapture.mm lacks request"
grep -q 'MacScreenCapture.mm' "$repo_root/cmake/macos/macos.cmake" || fail "macos.cmake does not list MacScreenCapture.mm"
for f in windows/windows.cmake linux/linux.cmake; do
  ! grep -q 'MacScreenCapture' "$repo_root/cmake/$f" || fail "$f names MacScreenCapture"
done
grep -q 'QString MacScreenRecordingMessage' "$pol" || fail "MacScreenRecordingMessage not defined in QrScanPolicy.cpp"
! grep -q 'Q_OS_' "$repo_root"/src/sys/macos/MacScreenCapture.* || fail "Q_OS_ in MacScreenCapture"
echo "mac_qr: OK"
