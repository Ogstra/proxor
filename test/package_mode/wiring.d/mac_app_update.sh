#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "mac_app_update.sh: $*" >&2; exit 1; }

grpc="$repo_root/src/ui/mainwindow_grpc.cpp"
win="$repo_root/src/ui/mainwindow.cpp"

# In macOS-only code: true when the needle sits between #ifdef Q_OS_MACOS and the next #else/#endif.
in_macos_range() {
  awk -v needle="$1" '/^#ifdef Q_OS_MACOS/{m=1; next} /^#(else|endif)/{m=0} m && index($0, needle){f=1} END{exit !f}' "$2"
}

in_macos_range 'macAppUpdateRoute(mode)' "$grpc" || fail "mainwindow_grpc.cpp must pick the route inside #ifdef Q_OS_MACOS"
in_macos_range 'request2.set_download_dir(mac_app_update_zip_dir' "$grpc" || fail "mainwindow_grpc.cpp must send download_dir inside #ifdef Q_OS_MACOS"
in_macos_range 'request2.set_channel(PackageModeName(mode)' "$grpc" || fail "mainwindow_grpc.cpp must send the channel inside #ifdef Q_OS_MACOS"
in_macos_range 'packageUpdate.allowDownload || macRoute != ProxorPlatform::MacAppUpdateRoute::Guidance' "$grpc" \
  || fail "the macOS Download condition must sit inside #ifdef Q_OS_MACOS"
grep -qxF '            if (dlg->chosenAction() == DialogUpdateAvailable::Download && allowSelfUpdate && packageUpdate.allowDownload) {' "$grpc" \
  || fail "the original Download condition line must stay for the other platforms"
grep -qxF '        const auto guidance = UpdateGuidanceText(mode, assetName);' "$grpc" || fail "the original guidance line must stay"

in_macos_range 'DecideMacAppUpdate(' "$win" || fail "mainwindow.cpp must decide the route inside #ifdef Q_OS_MACOS"
in_macos_range 'MacAppUpdateInstallArgs(' "$win" || fail "mainwindow.cpp must build the installer arguments inside #ifdef Q_OS_MACOS"
in_macos_range 'QStringLiteral("/bin/bash")' "$win" || fail "mainwindow.cpp must start the relauncher inside #ifdef Q_OS_MACOS"
in_macos_range 'exit_reason == 4' "$win" || fail "mainwindow.cpp must hand over on exit_reason 4 inside #ifdef Q_OS_MACOS"
in_macos_range 'ParseMacAppUpdateResult(' "$win" || fail "mainwindow.cpp must read the result inside #ifdef Q_OS_MACOS"
in_macos_range 'macAppUpdateShowResult(); });' "$win" || fail "the constructor must show the result inside #ifdef Q_OS_MACOS"

base_count=3
[ "$(grep -c 'UpdateFailureStage::' "$grpc")" = "$base_count" ] || fail "mainwindow_grpc.cpp must keep exactly $base_count UpdateFailureStage:: lines (macOS goes through macAppUpdateFailed)"

grep -qF 'brew upgrade --cask proxor' "$repo_root/src/main/PackagePolicy.cpp" || fail "PackagePolicy.cpp must keep the brew command"
grep -qF 'replace Proxor.app' "$repo_root/src/main/PackagePolicy.cpp" || fail "PackagePolicy.cpp must keep the manual replace text"
grep -qF 'MACOSX_PACKAGE_LOCATION Resources/update' "$repo_root/cmake/macos/macos.cmake" || fail "macos.cmake must bundle the relauncher under Resources/update"
if grep -rnE 'PROXOR_APP_UPDATE_' "$repo_root/src" >/dev/null 2>&1; then fail "relauncher test seams must not be referenced from src/"; fi
for d in windows linux; do
  if [ -d "$repo_root/cmake/$d" ] && grep -rqF 'MacAppUpdate' "$repo_root/cmake/$d"; then fail "cmake/$d must not mention MacAppUpdate"; fi
done
echo "mac_app_update: OK"
