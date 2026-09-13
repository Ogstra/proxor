#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
grep -q 'DecidePackageUpdate' "$repo_root/src/ui/mainwindow_grpc.cpp"
# allowUpdaterLaunch itself is consulted via DecideUpdaterLaunch/ProbeUpdaterLaunch (the
# filesystem-backed gate below, asserted separately); CheckUpdate's own self-update gate
# reads allowDownload.
grep -q 'allowDownload' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'DecideFlatpakLifecycle' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'StartVPNProcess' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'CoreAssetSearchPaths' "$repo_root/src/main/ProxorGui.cpp"
grep -q 'PackageMode.cpp' "$repo_root/CMakeLists.txt"
grep -q 'DecideUpdaterLaunch' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'ProbeUpdaterLaunch' "$repo_root/src/main/ProxorGui.cpp"
grep -q 'UpdateGuidanceText' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'PackageModeName' "$repo_root/src/main/main.cpp"
grep -q 'dialog_update_available_test' "$repo_root/test/package_mode/CMakeLists.txt"
