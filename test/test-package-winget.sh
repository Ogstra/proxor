#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
packer="$repo_root/libs/package_winget.sh"
release_packer="$repo_root/libs/package_release.sh"

test -x "$release_packer" || { printf '%s\n' 'direct package packer is missing' >&2; exit 1; }
test -x "$packer" || { printf '%s\n' 'winget package packer is missing' >&2; exit 1; }
command -v unzip >/dev/null || { printf '%s\n' 'unzip is required for the fixture' >&2; exit 1; }

work="$(mktemp -d "${TMPDIR:-/tmp}/proxor-winget-package.XXXXXX")"
trap 'rm -rf "$work"' EXIT
source_tree="$work/windows64"
mkdir -p "$source_tree/config/runtime/plugins/platforms" "$source_tree/config"
printf 'gui\n' > "$source_tree/proxor.exe"
printf 'core\n' > "$source_tree/proxor_core.exe"
printf 'runtime\n' > "$source_tree/config/runtime/Qt6Core.dll"
printf 'plugin\n' > "$source_tree/config/runtime/plugins/platforms/qwindows.dll"
printf 'log must be excluded\n' > "$source_tree/fixture.log"

direct_zip="$work/proxor-1.6.4-windows64.zip"
winget_zip="$work/proxor-1.6.4-winget-x64.zip"
"$release_packer" "$source_tree" "$direct_zip"
"$packer" "$source_tree" "$winget_zip"

test -s "$direct_zip"
test -s "$winget_zip"
case "$(basename "$winget_zip")" in *-winget-x64.zip) ;; *) exit 1 ;; esac
case "$(basename "$winget_zip")" in *windows64.zip) exit 1 ;; esac

direct_entries="$(unzip -Z1 "$direct_zip")"
winget_entries="$(unzip -Z1 "$winget_zip")"
printf '%s\n' "$direct_entries" | grep -qx 'proxor/proxor.exe'
printf '%s\n' "$direct_entries" | grep -q 'package-manager/winget' && exit 1
printf '%s\n' "$winget_entries" | grep -qx 'proxor/proxor.exe'
printf '%s\n' "$winget_entries" | grep -qx 'proxor/proxor_core.exe'
printf '%s\n' "$winget_entries" | grep -qx 'proxor/config/runtime/Qt6Core.dll'
printf '%s\n' "$winget_entries" | grep -qx 'proxor/config/runtime/plugins/platforms/qwindows.dll'
printf '%s\n' "$winget_entries" | grep -qx 'proxor/config/package-manager/winget'
test "$(printf '%s\n' "$winget_entries" | grep -c 'package-manager/winget')" -eq 1
printf '%s\n' "$winget_entries" | grep -q 'fixture.log' && exit 1

test ! -e "$source_tree/config/package-manager/winget"
test "$(unzip -p "$winget_zip" proxor/config/package-manager/winget)" = 'winget-managed-install'
