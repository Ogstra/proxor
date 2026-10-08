#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "mac_sleep_wake.sh: $*" >&2; exit 1; }
mm="$repo_root/src/sys/macos/MacSleepWake.mm"
cm="$repo_root/cmake/macos/macos.cmake"

grep -q 'src/sys/macos/MacSleepWake.mm' "$cm" || fail "macos.cmake must list MacSleepWake.mm"
grep 'fobjc-arc' "$cm" | grep -q 'src/sys/macos/MacSleepWake.mm' || fail "MacSleepWake.mm must be on the -fobjc-arc line"
for f in cmake/windows/windows.cmake cmake/linux/linux.cmake CMakeLists.txt; do
  [ -f "$repo_root/$f" ] || continue
  if grep -q 'MacSleepWake' "$repo_root/$f"; then fail "MacSleepWake must be named only by macos.cmake ($f)"; fi
done
grep -q 'NSWorkspaceWillSleepNotification' "$mm" || fail "observer must use NSWorkspaceWillSleepNotification"
grep -q 'NSWorkspaceDidWakeNotification' "$mm" || fail "observer must use NSWorkspaceDidWakeNotification"
grep -q 'queue:nil' "$mm" || fail "observer must register with queue:nil (synchronous WillSleep)"
if grep -n 'Q_OS_' "$mm"; then fail "MacSleepWake.mm must be OS-free (listed only in macos.cmake)"; fi

# Tests must never put the Mac to sleep (patterns built from split strings so this script does not match itself).
pm="pm""set"; io="IOPM""SleepSystem"; sc="systemctl ""suspend"
if grep -nE "$pm|$io|$sc" "$repo_root/test/package_mode/mac_sleep_wake_smoke_test.cpp" \
    "$repo_root/test/package_mode/mac_sleep_wake_post.mm" "$mm"; then
  fail "sleep-inducing call found"
fi
echo "mac_sleep_wake: OK"
