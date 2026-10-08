#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "mac_update.sh: $*" >&2; exit 1; }

go_file="$repo_root/go/grpc_server/update.go"
cask="$repo_root/packaging/homebrew/proxor.rb.in"
# The core and the cask must agree on the macOS asset name.
grep -qF '"-macos-arm64.zip"' "$go_file" || fail "update.go must resolve the -macos-arm64.zip asset"
grep -E '^[[:space:]]*url ' "$cask" | grep -qF -- '-macos-arm64.zip"' || fail "the cask url must end with -macos-arm64.zip"

grep -qF 'brew upgrade --cask proxor' "$repo_root/src/main/PackagePolicy.cpp" || fail "PackagePolicy.cpp must name the brew command"

# In macOS-only code: true when the needle sits between #ifdef Q_OS_MACOS and the next #else/#endif.
in_macos_range() {
  awk -v needle="$1" '/^#ifdef Q_OS_MACOS/{m=1; next} /^#(else|endif)/{m=0} m && index($0, needle){f=1} END{exit !f}' "$2"
}
in_macos_range 'DetectMacPackageMode(' "$repo_root/src/main/ProxorGui.cpp" || fail "ProxorGui.cpp must call DetectMacPackageMode( inside #ifdef Q_OS_MACOS"
in_macos_range 'UpdateIncludesPrereleases(' "$repo_root/src/ui/mainwindow_grpc.cpp" || fail "mainwindow_grpc.cpp must call UpdateIncludesPrereleases( inside #ifdef Q_OS_MACOS"
in_macos_range 'PrereleaseSettingNote(' "$repo_root/src/ui/dialog_basic_settings.cpp" || fail "dialog_basic_settings.cpp must call PrereleaseSettingNote( inside #ifdef Q_OS_MACOS"

# The non-macOS path keeps the original line.
grep -qF 'request.set_check_pre_release(ProxorGui::dataStore->check_include_pre);' "$repo_root/src/ui/mainwindow_grpc.cpp" \
  || fail "mainwindow_grpc.cpp lost the original set_check_pre_release line"
# Intel: the core and the two-architecture cask agree on the x86_64 asset; render and bump know about it.
two="$repo_root/packaging/homebrew/proxor-two-arch.rb.in"
grep -qF '"-macos-x86_64.zip"' "$go_file" || fail "update.go must resolve the -macos-x86_64.zip asset"
grep -E '^[[:space:]]*url ' "$two" | grep -qF -- '-macos-#{arch}.zip"' || fail "the two-architecture cask url must end with -macos-#{arch}.zip"
grep -qF 'intel: "x86_64"' "$two" || fail "the two-architecture cask must map intel to x86_64"
grep -qF -- '--sha256-intel' "$repo_root/packaging/homebrew/render-cask.sh" || fail "render-cask.sh must accept --sha256-intel"
grep -qF 'macos-x86_64.zip' "$repo_root/packaging/homebrew/bump-tap.sh" || fail "bump-tap.sh must look for the macos-x86_64.zip asset"
echo "mac_update: OK"
