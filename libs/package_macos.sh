#!/bin/bash
# Zip the Proxor.app produced by libs/build_macos.sh and prove the zip is safe to ship.
#
# Homebrew extracts a cask zip with /usr/bin/unzip (not ditto), so this script re-extracts the
# result the same way and re-verifies the ad-hoc signature. It also refuses to finish when a
# bundled Mach-O needs a newer macOS than the declared floor.
#
# Run from the repository root, after ./libs/build_macos.sh.
# Env knobs: MACOS_FLOOR (minimum macOS the bundle may require; default:
# MACOSX_DEPLOYMENT_TARGET, else 15.0), PROXOR_MACOS_ARCH (arm64 by default, or
# x86_64 to package the Intel app from libs/build_macos_intel.sh with extra
# x86_64-only architecture and minos checks).
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

arch="${PROXOR_MACOS_ARCH:-arm64}"
case "$arch" in
  arm64) build_script=./libs/build_macos.sh ;;
  x86_64) build_script=./libs/build_macos_intel.sh ;;
  *)
    echo "ERROR: unsupported PROXOR_MACOS_ARCH '$arch' (use arm64 or x86_64)." >&2
    exit 1
    ;;
esac

APP="$DEPLOYMENT/macos-$arch/Proxor.app"
if [ ! -d "$APP" ]; then
  echo "ERROR: $APP not found; run $build_script first." >&2
  exit 1
fi
version="$(tr -d '\n\r' < VERSION.txt)"
zip="$DEPLOYMENT/$version_standalone-macos-$arch.zip"
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

#### Intel architecture gate ####
# Only for PROXOR_MACOS_ARCH=x86_64: every Mach-O must be thin x86_64 and the executables must
# declare the expected minimum macOS. A function: bash 3.2 mis-parses `case` inside $( ).
list_non_x86_64() {
  local f kind
  find "$APP" -type f -print0 | while IFS= read -r -d '' f; do
    kind="$(file -b "$f")"
    if [ "${kind#Mach-O}" != "$kind" ] && [ "$(lipo -archs "$f" 2>/dev/null)" != "x86_64" ]; then
      echo "$f ($(lipo -archs "$f" 2>/dev/null))"
    fi
  done
}
bin_minos() {
  vtool -arch x86_64 -show-build "$1" | awk '/minos/ {print $2; exit}'
}
if [ "$arch" = x86_64 ]; then
  offenders="$(list_non_x86_64)"
  if [ -n "$offenders" ]; then
    echo "ERROR: Mach-O files that are not x86_64 only:" >&2
    printf '%s\n' "$offenders" >&2
    exit 1
  fi
  main_minos="$(bin_minos "$APP/Contents/MacOS/Proxor")"
  core_minos="$(bin_minos "$APP/Contents/MacOS/proxor_core")"
  echo "Proxor minos $main_minos, proxor_core minos $core_minos (floor $floor)"
  if [ "$main_minos" != "$floor" ]; then
    echo "ERROR: Proxor minos is '$main_minos', expected $floor" >&2
    exit 1
  fi
  if [ -z "$core_minos" ] || [ "$(printf '%s\n%s\n' "$core_minos" "$floor" | sort -V | tail -1)" != "$floor" ]; then
    echo "ERROR: proxor_core minos '$core_minos' is above the floor $floor" >&2
    exit 1
  fi
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
if ! grep -qxF 'Proxor.app/Contents/Resources/helper/helper-install.sh' <<<"$entries"; then
  echo "ERROR: zip does not contain Proxor.app/Contents/Resources/helper/helper-install.sh" >&2
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
test -x "$check/Proxor.app/Contents/Resources/helper/helper-install.sh"
test -x "$check/Proxor.app/Contents/Resources/helper/helper-uninstall.sh"
test -L "$check/Proxor.app/Contents/MacOS/geosite.db"
test -s "$check/Proxor.app/Contents/MacOS/geosite.db"
main_refs="$(otool -L "$check/Proxor.app/Contents/MacOS/Proxor")"
if grep -q /opt/homebrew <<<"$main_refs"; then
  echo "ERROR: Proxor still links against /opt/homebrew" >&2
  exit 1
fi

if [ "$arch" = x86_64 ]; then
  # The extracted bundle must not lean on a build machine's Homebrew or /usr/local libraries.
  bad_refs="$(find "$check/Proxor.app" -type f -print0 | while IFS= read -r -d '' f; do
    kind="$(file -b "$f")"
    if [ "${kind#Mach-O}" != "$kind" ]; then
      otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -E '^(/usr/local/|/opt/homebrew)' | sed "s|^|$f: |" || true
    fi
  done)"
  if [ -n "$bad_refs" ]; then
    echo "ERROR: x86_64 bundle references Homebrew or /usr/local libraries:" >&2
    printf '%s\n' "$bad_refs" >&2
    exit 1
  fi
  echo "INTEL-ARCH-OK (x86_64, minos $floor)"
fi

shasum -a 256 "$zip"
echo "macOS release asset ready: $zip"
