#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
grep -q 'DecidePackageUpdate' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'allowUpdaterLaunch' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'DecideFlatpakLifecycle' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'StartVPNProcess' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'CoreAssetSearchPaths' "$repo_root/src/main/ProxorGui.cpp"
grep -q 'PackageMode.cpp' "$repo_root/CMakeLists.txt"
grep -q 'DecideUpdaterLaunch' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'ProbeUpdaterLaunch' "$repo_root/src/main/ProxorGui.cpp"
