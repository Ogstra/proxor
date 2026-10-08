#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
hpp="$repo_root/src/platform/WakeCoordinator.hpp"
cpp="$repo_root/src/platform/WakeCoordinator.cpp"
facade="$repo_root/src/sys/SleepWake.hpp"

if grep -n 'Q_OS_' "$hpp" "$cpp" "$facade"; then echo "wake.sh: Q_OS_ in the pure wake module or the facade" >&2; exit 1; fi
if grep -nE 'QTimer|ProxorGui::|dataStore|MainWindow|QtWidgets' "$hpp" "$cpp"; then echo "wake.sh: WakeCoordinator must stay pure (no QTimer/app globals/widgets)" >&2; exit 1; fi
grep -q 'kWakeDedupeMs = 120000' "$hpp" || { echo "wake.sh: kWakeDedupeMs = 120000 missing" >&2; exit 1; }
grep -q 'kWakeNetworkWaitMaxMs = 60000' "$hpp" || { echo "wake.sh: kWakeNetworkWaitMaxMs = 60000 missing" >&2; exit 1; }
grep -q 'namespace ProxorSleepWake' "$facade" || { echo "wake.sh: namespace ProxorSleepWake missing" >&2; exit 1; }
grep -q 'InstallResult Install(QObject \*owner' "$facade" || { echo "wake.sh: Install facade signature missing" >&2; exit 1; }
if grep -q 'SleepWake' "$repo_root/cmake/windows/windows.cmake"; then echo "wake.sh: windows.cmake must not mention SleepWake" >&2; exit 1; fi
echo "wake: OK"
