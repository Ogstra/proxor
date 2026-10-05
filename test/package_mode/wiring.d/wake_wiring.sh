#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "wake_wiring.sh: $*" >&2; exit 1; }
mw="$repo_root/src/ui/mainwindow.cpp"
wk="$repo_root/src/ui/mainwindow_wake.cpp"
[ -f "$wk" ] || fail "src/ui/mainwindow_wake.cpp is missing"

# Timer gap: native/timer path inside the macOS+Linux guard, the old call kept in the #else branch.
awk '
/^#if defined\(Q_OS_MACOS\) \|\| defined\(Q_OS_LINUX\)/ { st = 1; seen = 0; next }
/^#else/ { if (st == 1) st = 2; next }
/^#endif/ { st = 0; next }
st == 1 && /wakeDetected\(ProxorPlatform::WakeSource::TimerGap\)/ { seen = 1 }
st == 2 && seen && /queue_resume_subscription_check\(\);/ { ok = 1 }
END { exit !ok }' "$mw" || fail "mainwindow.cpp: timer gap must call wakeDetected(TimerGap) under the macOS/Linux guard and queue_resume_subscription_check() in the #else"
awk '
/^#if defined\(Q_OS_MACOS\) \|\| defined\(Q_OS_LINUX\)/ { st = 1; next }
/^#else/ { st = 0; next }
/^#endif/ { st = 0; next }
st == 1 && /wakeInstall\(\);/ { ok = 1 }
END { exit !ok }' "$mw" || fail "mainwindow.cpp: wakeInstall() must sit under the macOS/Linux guard"
n=$(grep -c 'wakeOwnsSubscriptions()' "$mw" || true)
[ "$n" -ge 2 ] || fail "mainwindow.cpp: wakeOwnsSubscriptions() must be used by the tick and queue_resume_subscription_check"
awk '/MainWindow::applicationStateChanged/ { f = 1 } f && /queue_resume_subscription_check\(\)/ { ok = 1 } END { exit !ok }' "$mw" \
  || fail "applicationStateChanged must still call queue_resume_subscription_check()"

for s in 'ProxorSleepWake::Install' 'wake_coord.noteSleep' 'wake_coord.noteWake' 'proxor_start(' \
         'UI_update_due_groups_on_timer()' 'UI_has_due_subscription_updates()' 'UI_subscription_updates_running()' \
         'start_pending' 'MacHelperSvc()->status('; do
  grep -qF "$s" "$wk" || fail "mainwindow_wake.cpp must contain $s"
done
for s in 'MacHelperSvc()->tunStart' 'MacHelperSvc()->sysproxyApply' 'macInstallHelperThen'; do
  if grep -qF "$s" "$wk"; then fail "mainwindow_wake.cpp must not contain $s (use the existing apply paths)"; fi
done

users=$(grep -rl 'UI_has_due_subscription_updates' "$repo_root/src" | sed "s#^$repo_root/##" | sort | tr '\n' ' ')
for f in $users; do
  case "$f" in
    src/sub/GroupUpdater.*|src/ui/mainwindow_wake.cpp) ;;
    *) fail "UI_has_due_subscription_updates must only be used by GroupUpdater and mainwindow_wake.cpp (found $f)" ;;
  esac
done

grep -q 'src/ui/mainwindow_wake.cpp' "$repo_root/cmake/macos/macos.cmake" || fail "macos.cmake must list mainwindow_wake.cpp"
grep -q 'src/ui/mainwindow_wake.cpp' "$repo_root/cmake/linux/linux.cmake" || fail "linux.cmake must list mainwindow_wake.cpp"
for f in cmake/windows/windows.cmake CMakeLists.txt; do
  [ -f "$repo_root/$f" ] || continue
  if grep -q 'mainwindow_wake' "$repo_root/$f"; then fail "mainwindow_wake must not be in $f"; fi
done
if [ -f "$repo_root/cmake/windows/windows.cmake" ] && grep -qE 'SleepWake|LogindSleep|MacSleepWake|mainwindow_wake' "$repo_root/cmake/windows/windows.cmake"; then
  fail "windows.cmake must not mention the sleep/wake sources"
fi

# This phase's code never puts a machine to sleep (patterns built from pieces so this file does not hold the literals).
pm="pm""set"; io="IOPM""SleepSystem"; sc="systemctl ""suspend"
for f in src/ui/mainwindow_wake.cpp src/sys/macos/MacSleepWake.mm src/sys/linux/LogindSleep.hpp src/sys/linux/LogindSleep.cpp \
         test/package_mode/mac_sleep_wake_smoke_test.cpp test/package_mode/mac_sleep_wake_post.mm test/package_mode/logind_sleep_test.cpp; do
  [ -f "$repo_root/$f" ] || continue
  if grep -nE "$pm|$io|$sc" "$repo_root/$f"; then fail "$f must not put the machine to sleep"; fi
done
echo "wake_wiring: OK"
