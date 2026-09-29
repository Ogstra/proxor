#!/bin/bash
# Zip the Proxor.app produced by libs/build_macos.sh and prove the zip is safe to ship.
#
# Homebrew extracts a cask zip with /usr/bin/unzip (not ditto), so this script re-extracts the
# result the same way and re-verifies the ad-hoc signature. It also refuses to finish when a
# bundled Mach-O needs a newer macOS than the declared floor.
#
# Run from the repository root, after ./libs/build_macos.sh.
# Env knobs: MACOS_FLOOR (minimum macOS the bundle may require; default:
# MACOSX_DEPLOYMENT_TARGET, else 15.0).
set -euo pipefail

#### guards ####
if [ "$(uname -s)" != "Darwin" ]; then
  echo "ERROR: libs/package_macos.sh only runs on macOS." >&2
  exit 1
fi
if [ ! -f VERSION.txt ] || [ ! -f libs/env_deploy.sh ]; then
  echo "ERROR: run from the repository root." >&2
  exit 1
fi

source libs/env_deploy.sh

APP="$DEPLOYMENT/macos-arm64/Proxor.app"
if [ ! -d "$APP" ]; then
  echo "ERROR: $APP not found; run ./libs/build_macos.sh first." >&2
  exit 1
fi
version="$(tr -d '\n\r' < VERSION.txt)"
zip="$DEPLOYMENT/$version_standalone-macos-arm64.zip"
floor="${MACOS_FLOOR:-${MACOSX_DEPLOYMENT_TARGET:-15.0}}"

#### minimum macOS gate ####
# Every regular file (symlinks excluded) that is a Mach-O: list "<minos><TAB><path>" per load command.
# A function, not an inline case in $( ): bash 3.2 mis-parses `case ... )` inside command substitution.
list_minos() {
  local f kind
  find "$APP" -type f -print0 | while IFS= read -r -d '' f; do
    kind="$(file -b "$f")"
    if [ "${kind#Mach-O}" != "$kind" ]; then
      otool -l "$f" | awk -v path="$f" '
        /LC_BUILD_VERSION/ {b=1}
        b && /minos/ {print $2 "\t" path; b=0}
        /LC_VERSION_MIN_MACOSX/ {m=1}
        m && / version/ {print $2 "\t" path; m=0}'
    fi
  done
}
minos_list="$(list_minos)"

worst="$(printf '%s\n' "$minos_list" | cut -f1 | sed '/^$/d' | sort -V | tail -1)"
if [ -z "$worst" ]; then
  echo "ERROR: no Mach-O found in bundle $APP" >&2
  exit 1
fi
echo "highest minos in bundle: $worst (floor $floor)"
if [ "$(printf '%s\n%s\n' "$worst" "$floor" | sort -V | tail -1)" != "$floor" ]; then
  echo "ERROR: bundle needs macOS $worst > $floor" >&2
  printf '%s\n' "$minos_list" | while IFS="$(printf '\t')" read -r v p; do
    [ -n "$v" ] || continue
    if [ "$(printf '%s\n%s\n' "$v" "$floor" | sort -V | tail -1)" != "$floor" ]; then
      echo "  minos $v  $p" >&2
    fi
  done
  exit 1
fi

plist_min="$(/usr/libexec/PlistBuddy -c 'Print :LSMinimumSystemVersion' "$APP/Contents/Info.plist" 2>/dev/null || true)"
if [ "$plist_min" != "$floor" ]; then
  echo "ERROR: LSMinimumSystemVersion is '$plist_min', expected $floor" >&2
  exit 1
fi

#### package ####
rm -f "$zip"
ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$APP" "$zip"

#### listing checks ####
# List once, then grep the variable: an `unzip | grep -q` pipe dies of SIGPIPE under pipefail.
entries="$(unzip -Z1 "$zip")"
if grep -Eq '(^|/)(\._|__MACOSX)' <<<"$entries"; then
  echo "ERROR: zip contains AppleDouble or __MACOSX entries" >&2
  exit 1
fi
if ! grep -qxF 'Proxor.app/Contents/MacOS/Proxor' <<<"$entries"; then
  echo "ERROR: zip does not contain Proxor.app/Contents/MacOS/Proxor" >&2
  exit 1
fi
link_info="$(zipinfo "$zip" 'Proxor.app/Contents/MacOS/geosite.db')"
if ! grep -q '^l' <<<"$link_info"; then
  echo "ERROR: geosite.db is not stored as a symlink" >&2
  exit 1
fi

#### Homebrew-equivalent extraction ####
check="$(mktemp -d)"
trap 'rm -rf "$check"' EXIT
/usr/bin/unzip -q "$zip" -d "$check"
codesign --verify --deep --strict --verbose=2 "$check/Proxor.app"
test -x "$check/Proxor.app/Contents/MacOS/Proxor"
test -x "$check/Proxor.app/Contents/MacOS/proxor_core"
test -L "$check/Proxor.app/Contents/MacOS/geosite.db"
test -s "$check/Proxor.app/Contents/MacOS/geosite.db"
main_refs="$(otool -L "$check/Proxor.app/Contents/MacOS/Proxor")"
if grep -q /opt/homebrew <<<"$main_refs"; then
  echo "ERROR: Proxor still links against /opt/homebrew" >&2
  exit 1
fi

shasum -a 256 "$zip"
echo "macOS release asset ready: $zip"
