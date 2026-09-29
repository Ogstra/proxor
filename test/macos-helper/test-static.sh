#!/usr/bin/env bash
# Static checks for the macOS helper install/uninstall scripts. Runs on Linux and macOS, never as
# root, and touches only a mktemp dir. The scripts are never executed past their argument
# validation / root check.
set -euo pipefail

root="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"
install_sh="$root/packaging/macos/helper-install.sh"
uninstall_sh="$root/packaging/macos/helper-uninstall.sh"

if [ "$(id -u)" = 0 ]; then
  echo "FAIL: test-static.sh must not run as root" >&2
  exit 1
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() { echo "FAIL: $*" >&2; exit 1; }

# Snapshot of the system paths the install script would create, to prove nothing is touched.
watched=(
  /Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper
  /Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper.new
  /Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist
  /Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist.new
  "/Library/Application Support/Proxor"
)
before=""
for p in "${watched[@]}"; do
  if [ -e "$p" ]; then before="$before|$p"; fi
done

#### syntax ####
sh -n "$install_sh"
sh -n "$uninstall_sh"
if command -v shellcheck >/dev/null 2>&1; then
  shellcheck -s sh "$install_sh" "$uninstall_sh"
fi

#### argument rejection (unprivileged, must exit 2 before any change) ####
good_sha="$(printf x | shasum -a 256 | awk '{print $1}')"
good_src="$tmp/src-bin"
printf 'not a real binary\n' >"$good_src"
good_app="$tmp/Fake.app"
mkdir -p "$good_app"
link_src="$tmp/src-link"
ln -s "$good_src" "$link_src"
not_app="$tmp/NotAnApp"
mkdir -p "$not_app"

expect_status() {
  local want="$1"; shift
  local got=0
  "$install_sh" "$@" >/dev/null 2>"$tmp/stderr" || got=$?
  if [ "$got" != "$want" ]; then
    fail "expected exit $want, got $got for args: $* ($(cat "$tmp/stderr"))"
  fi
}

# uid
expect_status 2 0 "$good_src" "$good_sha" "$good_app"
expect_status 2 abc "$good_src" "$good_sha" "$good_app"
expect_status 2 -1 "$good_src" "$good_sha" "$good_app"
expect_status 2 "" "$good_src" "$good_sha" "$good_app"
# sha256
upper_sha="$(printf %s "$good_sha" | tr a-f A-F)"
# An all-digit hash has no letters to uppercase; make sure the case really differs.
if [ "$upper_sha" = "$good_sha" ]; then upper_sha="${good_sha%?}G"; fi
expect_status 2 501 "$good_src" "$upper_sha" "$good_app"
expect_status 2 501 "$good_src" "${good_sha%?}" "$good_app"
# src
expect_status 2 501 "relative/path" "$good_sha" "$good_app"
expect_status 2 501 "$tmp/does-not-exist" "$good_sha" "$good_app"
expect_status 2 501 "$link_src" "$good_sha" "$good_app"
expect_status 2 501 "$tmp" "$good_sha" "$good_app"
# app path
expect_status 2 501 "$good_src" "$good_sha" "$not_app"
expect_status 2 501 "$good_src" "$good_sha" "$tmp/Missing.app"
expect_status 2 501 "$good_src" "$good_sha" "relative/Fake.app"
nl_app="$tmp/New"$'\n'"line.app"
mkdir -p "$nl_app"
expect_status 2 501 "$good_src" "$good_sha" "$nl_app"
# argument count
expect_status 2 501 "$good_src" "$good_sha"
expect_status 2 501 "$good_src" "$good_sha" "$good_app" extra
expect_status 2

# Valid arguments as a non-root user: validation passed, so the root check answers.
expect_status 5 501 "$good_src" "$good_sha" "$good_app"

# Uninstall without root.
got=0
"$uninstall_sh" >/dev/null 2>&1 || got=$?
[ "$got" = 5 ] || fail "helper-uninstall.sh unprivileged exit $got, expected 5"

# Nothing appeared under /Library.
after=""
for p in "${watched[@]}"; do
  if [ -e "$p" ]; then after="$after|$p"; fi
done
[ "$before" = "$after" ] || fail "system paths changed while running unprivileged: before='$before' after='$after'"

#### validation precedes the root check ####
last_validation="$(grep -n 'die 2' "$install_sh" | tail -1 | cut -d: -f1)"
first_root_check="$(grep -n 'id -u' "$install_sh" | head -1 | cut -d: -f1)"
[ -n "$last_validation" ] && [ -n "$first_root_check" ] || fail "could not locate validation or root check lines"
# The symlink refusal for the /Library dirs is deliberately after the root check; only the
# argument validation (everything before it) must precede it.
arg_validation_last="$(grep -n 'die 2 "app path is not a directory"' "$install_sh" | cut -d: -f1)"
[ -n "$arg_validation_last" ] || fail "app path validation line missing"
[ "$arg_validation_last" -lt "$first_root_check" ] || fail "root check precedes argument validation"

#### embedded plist ####
plist="$tmp/helper.plist"
awk '
  /^# BEGIN PLIST$/ {in_block = 1; next}
  /^# END PLIST$/   {in_block = 0}
  in_block && /<<.PLIST.$/ {in_doc = 1; next}
  in_block && /^PLIST$/    {in_doc = 0; next}
  in_block && in_doc {print}
' "$install_sh" >"$plist"
[ -s "$plist" ] || fail "no plist extracted between BEGIN/END PLIST markers"
[ "$(grep -c '^# BEGIN PLIST$' "$install_sh")" = 1 ] || fail "expected exactly one BEGIN PLIST marker"
[ "$(grep -c '^# END PLIST$' "$install_sh")" = 1 ] || fail "expected exactly one END PLIST marker"

if [ "$(uname -s)" = Darwin ]; then
  plutil -lint "$plist" >/dev/null || fail "plutil -lint rejected the embedded plist"
fi
for needle in \
  '<string>io.github.Ogstra.Proxor.helper</string>' \
  '/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper' \
  '<string>helper</string>' \
  'AssociatedBundleIdentifiers'; do
  grep -qF -- "$needle" "$plist" || fail "plist is missing: $needle"
done

#### forbidden constructs ####
for f in "$install_sh" "$uninstall_sh"; do
  if grep -Eq '\$HOME|SUDO_UID|(^|[^A-Za-z_])eval([^A-Za-z_]|$)|curl' "$f"; then
    fail "$(basename "$f") contains a forbidden construct (\$HOME, SUDO_UID, eval or curl)"
  fi
done

echo "test-static.sh: OK"
