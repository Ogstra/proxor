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
grep -q 'MacHelperSvc()->tunStart' "$repo_root/src/ui/mainwindow.cpp"
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
grep -q 'MacHelperSvc()->sysproxyApply' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelperSvc()->sysproxyRestore' "$repo_root/src/ui/mainwindow.cpp"
# Pause/resume of System Proxy and Tun on a user stop goes through MacModeCoordinator (grace period, no flap).
grep -q 'mac_modes->profileStopping(sem, ProxorGui::dataStore->prepare_exit)' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'mac_modes->profileStopped()' "$repo_root/src/ui/mainwindow_grpc.cpp"
if ! awk '/^void MainWindow::proxor_stop/,/^}/' "$repo_root/src/ui/mainwindow_grpc.cpp" \
    | awk '/mu_stopping.unlock\(\)/{u=NR} /mac_modes->profileStopped\(\)/{p=NR} END{exit !(u && p && u < p)}'; then
  echo "test-policy-wiring.sh: profileStopped must run after the stop stage, or a slow stop drops the pause" >&2
  exit 1
fi
grep -q 'mac_modes->profileStarting()' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'mac_modes->profileStarted()' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'mac_modes->profileStartFailed()' "$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'macPauseModes' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'macResumeModes' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelperSvc()->tunStop' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'new MacModeCoordinator' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'src/sys/macos/MacModeCoordinator.cpp' "$repo_root/cmake/macos/macos.cmake"
grep -q 'mac_mode_coordinator_test' "$repo_root/test/package_mode/CMakeLists.txt"
# The pause keeps Tun Mode checked and remembered: it must not touch remember_spmode or switch Tun off.
if awk '/^void MainWindow::macPauseModes/,/^}/' "$repo_root/src/ui/mainwindow.cpp" | grep -qE 'remember_spmode|proxor_set_spmode_vpn\(false'; then
  echo "test-policy-wiring.sh: macPauseModes must keep Tun Mode checked and remembered" >&2
  exit 1
fi
# The GUI never waits for the helper: async facade call sites, async startup probe, bounded exit close.
grep -q 'MacHelperSvc()->probe' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'macStartupProbed' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelperSvc()->shutdown' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'src/sys/macos/MacHelperService.cpp' "$repo_root/cmake/macos/macos.cmake"
if grep -rnE 'MacHelper\(\)' "$repo_root/src" >/dev/null; then
  echo "test-policy-wiring.sh: the synchronous helper singleton is back" >&2
  exit 1
fi
# Launch install prompt (50-19): asks once per session, never holds the profile, one installer at a time.
grep -q 'DecideMacStartupInstall' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacStartupInstallDeclinedText' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'mac_install_prompted_this_session' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelperInstaller::InstallInProgress()' "$repo_root/src/ui/mainwindow.cpp"
grep -q 'MacHelperInstaller::InstallInProgress()' "$repo_root/src/ui/dialog_vpn_settings.cpp"
grep -q 'g_installInProgress' "$repo_root/src/sys/macos/MacHelperInstaller.cpp"
if ! awk '/^void MainWindow::macStartupProbed/,/^}/' "$repo_root/src/ui/mainwindow.cpp" \
    | awk '/resumeDeferredStartupProfile/{r=NR} /ConfirmAndInstall/{c=NR} END{exit !(r && c && r < c)}'; then
  echo "test-policy-wiring.sh: startup install prompt must not hold the profile" >&2
  exit 1
fi
# A non-user Tun loss keeps Tun remembered (only the user's own toggle or Remove un-remembers it).
if awk '/^void MainWindow::macOnTunStopped/,/^}/' "$repo_root/src/ui/mainwindow.cpp" | grep -q 'proxor_set_spmode_vpn(false);'; then
  echo "test-policy-wiring.sh: macOnTunStopped must not un-remember Tun (use save=false)" >&2
  exit 1
fi
grep -q 'Settings > Tun settings' "$repo_root/docs/Build_macOS.md"
