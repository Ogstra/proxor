#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "logind_sleep.sh: $1" >&2; exit 1; }

lin="$repo_root/cmake/linux/linux.cmake"
sys="$repo_root/src/sys"
cpp="$sys/linux/LogindSleep.cpp"
hpp="$sys/linux/LogindSleep.hpp"

grep -qF 'src/sys/linux/LogindSleep.cpp' "$lin" || fail "linux.cmake must list LogindSleep.cpp"
for f in "$repo_root/cmake/windows/windows.cmake" "$repo_root/cmake/macos/macos.cmake" "$repo_root/CMakeLists.txt"; do
  if grep -qF 'LogindSleep' "$f"; then fail "$f must not mention LogindSleep"; fi
done
for pat in PrepareForSleep org.freedesktop.login1.Manager 'systemBus()'; do
  grep -qF -- "$pat" "$cpp" "$hpp" || fail "LogindSleep.* must contain $pat"
done

# One Install definition per OS file (MacSleepWake.mm may not exist yet).
for f in "$cpp" "$sys/macos/MacSleepWake.mm"; do
  [ -f "$f" ] || continue
  n="$(grep -cE 'InstallResult Install\(' "$f" || true)"
  [ "$n" = 1 ] || fail "$f must define InstallResult Install( exactly once (found $n)"
done

if grep -n 'Q_OS_' "$cpp" "$hpp" >&2; then fail "no Q_OS_ in LogindSleep.*"; fi
if grep -nE 'systemctl|Suspend\(|Inhibit\(' "$cpp" "$hpp" >&2; then fail "LogindSleep.* only listens, it never acts"; fi
echo "logind_sleep: OK"
