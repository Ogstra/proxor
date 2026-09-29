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
grep -q 'set_channel' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'DecideAppImageApply' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'APPIMAGE' "$repo_root/src/ui/mainwindow.cpp"
# macOS Tun through the Proxor service (phase 50)
grep -q 'DecideMacTunStartup' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'DecideMacHelperEnable' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelper()->tunStart' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacTunFailureText' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'mac_stop_keeps_remembered_profile' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'src/sys/macos/MacHelperPolicy.cpp' "$repo_root/cmake/macos/macos.cmake"
# Fresh configs remember Tun by default; the macOS startup gate must read that same list (no fresh-config bypass).
grep -q 'QStringList remember_spmode = {"vpn"};' "$repo_root/src/main/ProxorGui_DataStore.hpp"
grep -q 'mac_remembered_vpn = ProxorGui::dataStore->remember_spmode.contains("vpn")' "$repo_root/src/ui/mainwindow.cpp"
# The earlier macOS Tun stopgap must not come back.
if grep -q 'Tun mode is not available on macOS yet' "$repo_root/src/ui/mainwindow.cpp"; then
  echo "test-policy-wiring.sh: the macOS Tun stopgap is back" >&2
  exit 1
fi
# macOS System Proxy through the Proxor service (phase 50): toggle, restore on user stop, re-apply on start
grep -q 'MacHelper()->sysproxyApply' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelper()->sysproxyRestore' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'macParkSystemProxy' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'macApplySystemProxy' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q '!sem && ProxorGui::dataStore->spmode_system_proxy && !ProxorGui::dataStore->prepare_exit' "$repo_root/src/ui/mainwindow_grpc.cpp"
