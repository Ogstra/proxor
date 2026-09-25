#!/bin/bash
# Offline guard for the third-party archive pins in libs/build_deps_all.sh.
#
# 1. Proves fetch_verified (libs/build_deps_fetch.sh) actually rejects a wrong
#    hash and actually accepts a right one, using a file:// URL so no network
#    is needed and the runner's flakiness cannot mask a bug here.
# 2. (extended by a later task) Proves libs/build_deps_all.sh and
#    packaging/flatpak/io.github.Ogstra.Proxor.yml agree on every pin.
set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=../build_deps_fetch.sh
. "$REPO_ROOT/libs/build_deps_fetch.sh"

FAILURES=0
fail() {
  echo "FAIL: $1" >&2
  FAILURES=$((FAILURES + 1))
}

command -v shasum >/dev/null 2>&1 || {
  echo "FAIL: shasum -a 256 is required on this platform" >&2
  exit 1
}

TMPDIR_T=$(mktemp -d)
trap 'rm -rf "$TMPDIR_T"' EXIT

# Build a file:// URL that also works on Windows Git Bash, where pwd -W would
# give a drive-letter path like C:/Users/... that needs a third leading slash.
to_file_url() {
  path=$(cd "$1" && pwd)
  case "$path" in
    [A-Za-z]:/*) echo "file:///$path" ;;
    /*) echo "file://$path" ;;
    *) echo "file:///$path" ;;
  esac
}

printf 'hello dependency pins' > "$TMPDIR_T/src.bin"
SRC_URL=$(to_file_url "$TMPDIR_T")/src.bin
REAL_HASH=$(shasum -a 256 "$TMPDIR_T/src.bin" | cut -d' ' -f1)
ZERO_HASH="0000000000000000000000000000000000000000000000000000000000000000"

# --- correct hash: succeeds, output exists with original bytes ---
if fetch_verified "$SRC_URL" "$REAL_HASH" "$TMPDIR_T/ok.bin"; then
  if ! cmp -s "$TMPDIR_T/src.bin" "$TMPDIR_T/ok.bin"; then
    fail "fetch_verified wrote different bytes than the source on a correct hash"
  fi
else
  fail "fetch_verified returned non-zero on a correct hash"
fi

# --- wrong hash: fails, stderr names both hashes, no output file left ---
STDERR_OUT=$(fetch_verified "$SRC_URL" "$ZERO_HASH" "$TMPDIR_T/bad.bin" 2>&1 1>/dev/null) && {
  fail "fetch_verified returned zero on a wrong hash"
}
if [ -e "$TMPDIR_T/bad.bin" ]; then
  fail "fetch_verified left bad.bin on disk after a checksum mismatch"
fi
case "$STDERR_OUT" in
  *"$REAL_HASH"*) : ;;
  *) fail "mismatch stderr did not contain the actual hash: $STDERR_OUT" ;;
esac
case "$STDERR_OUT" in
  *"$ZERO_HASH"*) : ;;
  *) fail "mismatch stderr did not contain the expected hash: $STDERR_OUT" ;;
esac

BUILD_DEPS="$REPO_ROOT/libs/build_deps_all.sh"
FLATPAK_MANIFEST="$REPO_ROOT/packaging/flatpak/io.github.Ogstra.Proxor.yml"

# --- drift guard: the three pins must match the Flatpak manifest's copies ---
# Match by URL, not by position, so the assertion cannot silently pass when a
# url: line moves or an entry is reordered.
check_pin_matches_manifest() {
  archive_name=$1
  var_url=$2
  var_sha=$3

  script_url=$(eval "echo \"\$$var_url\"")
  script_sha=$(eval "echo \"\$$var_sha\"")

  # The manifest's sha256: line immediately follows its matching url: line.
  manifest_sha=$(grep -A1 -F "url: $script_url" "$FLATPAK_MANIFEST" | grep 'sha256:' | head -1 | sed 's/.*sha256:[[:space:]]*//')

  if [ -z "$manifest_sha" ]; then
    fail "$archive_name: no matching url in $FLATPAK_MANIFEST for $script_url"
    return
  fi
  if [ "$manifest_sha" != "$script_sha" ]; then
    fail "$archive_name: build_deps_all.sh sha256 ($script_sha) does not match flatpak manifest ($manifest_sha) for $script_url"
  fi
}

# shellcheck disable=SC1090
ZXING_URL=$(grep '^ZXING_URL=' "$BUILD_DEPS" | sed 's/^ZXING_URL="\(.*\)"$/\1/')
ZXING_SHA256=$(grep '^ZXING_SHA256=' "$BUILD_DEPS" | sed 's/^ZXING_SHA256="\(.*\)"$/\1/')
YAMLCPP_URL=$(grep '^YAMLCPP_URL=' "$BUILD_DEPS" | sed 's/^YAMLCPP_URL="\(.*\)"$/\1/')
YAMLCPP_SHA256=$(grep '^YAMLCPP_SHA256=' "$BUILD_DEPS" | sed 's/^YAMLCPP_SHA256="\(.*\)"$/\1/')
PROTOBUF_URL=$(grep '^PROTOBUF_URL=' "$BUILD_DEPS" | sed 's/^PROTOBUF_URL="\(.*\)"$/\1/')
PROTOBUF_SHA256=$(grep '^PROTOBUF_SHA256=' "$BUILD_DEPS" | sed 's/^PROTOBUF_SHA256="\(.*\)"$/\1/')

check_pin_matches_manifest "zxing-cpp" ZXING_URL ZXING_SHA256
check_pin_matches_manifest "yaml-cpp" YAMLCPP_URL YAMLCPP_SHA256
check_pin_matches_manifest "protobuf" PROTOBUF_URL PROTOBUF_SHA256

# --- build_deps_all.sh must not clone git or bypass fetch_verified ---
if grep -q 'git clone' "$BUILD_DEPS"; then
  fail "libs/build_deps_all.sh still contains a git clone; protobuf must come from the pinned archive"
fi
if [ "$(grep -c 'curl ' "$BUILD_DEPS")" -ne 0 ]; then
  fail "libs/build_deps_all.sh calls curl directly instead of going through fetch_verified"
fi

if [ "$FAILURES" -ne 0 ]; then
  echo "$FAILURES check(s) failed" >&2
  exit 1
fi

echo "test-dependency-pins.sh: all checks passed"
