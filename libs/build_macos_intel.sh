#!/bin/bash
# Build, assemble, deploy and ad-hoc sign Proxor.app for Intel (x86_64) Macs, macOS 12 or later.
#
# Separate from libs/build_macos.sh (arm64, Homebrew Qt, untouched): Homebrew bottles target the
# runner's macOS, which cannot give a macOS 12 floor. This script uses the official Qt (aqtinstall
# clang_64, built for macOS 11+), static dependencies built by libs/build_deps_all.sh at the
# deployment target, and a darwin/amd64 proxor_core. Its assemble section is kept in step with
# build_macos.sh. The resulting app is never launched here.
#
# Run from the repository root. Env knobs:
#   PROXOR_QT_DIR            Qt prefix (.../6.7.2/macos) with lib/cmake/Qt6 and bin/macdeployqt
#                            (default: $QT_ROOT_DIR, required)
#   MACOSX_DEPLOYMENT_TARGET minimum macOS (default and exported: 12.0)
#   DEPS_PREFIX              static dependency prefix
#                            (default: $SRC_ROOT/libs/deps/macos-x86_64/built)
#   BUILD_DIR                CMake build tree (default: $SRC_ROOT/build-macos-x86_64)
#   SKIP_DEPS=1              do not build the dependencies
#   SKIP_GO=1                reuse an already-built deployment/macos-amd64/proxor_core
set -euo pipefail

source libs/env_deploy.sh

#### guards ####
if [ "$(uname -s)" != "Darwin" ]; then
  echo "ERROR: libs/build_macos_intel.sh only runs on macOS." >&2
  exit 1
fi

for tool in cmake ninja go codesign lipo vtool otool install_name_tool; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required tool not found on PATH: $tool" >&2
    exit 1
  fi
done

PROXOR_QT_DIR="${PROXOR_QT_DIR:-${QT_ROOT_DIR:-}}"
if [ -z "$PROXOR_QT_DIR" ] || [ ! -d "$PROXOR_QT_DIR/lib/cmake/Qt6" ] || [ ! -x "$PROXOR_QT_DIR/bin/macdeployqt" ]; then
  echo "ERROR: set PROXOR_QT_DIR (or QT_ROOT_DIR) to the official Qt prefix (.../6.7.2/macos)," >&2
  echo "  which must contain lib/cmake/Qt6 and bin/macdeployqt. Install it with:" >&2
  echo "  aqt install-qt mac desktop 6.7.2 clang_64" >&2
  exit 1
fi

MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-12.0}"
export MACOSX_DEPLOYMENT_TARGET

DEFAULT_DEPS_PREFIX="$SRC_ROOT/libs/deps/macos-x86_64/built"
DEPS_PREFIX="${DEPS_PREFIX:-$DEFAULT_DEPS_PREFIX}"
BUILD_DIR="${BUILD_DIR:-$SRC_ROOT/build-macos-x86_64}"
DEST="$DEPLOYMENT/macos-x86_64"
APP="$DEST/Proxor.app"
CORE_DIR="$DEPLOYMENT/macos-amd64"

#### dependencies ####
# build_deps_all.sh honours the `deps` (relative to libs/) and `cmake` shell variables.
if [ "${SKIP_DEPS:-0}" != "1" ] && [ ! -d "$DEPS_PREFIX/lib/cmake/protobuf" ]; then
  if [ "$DEPS_PREFIX" != "$DEFAULT_DEPS_PREFIX" ]; then
    echo "ERROR: DEPS_PREFIX $DEPS_PREFIX has no lib/cmake/protobuf; a custom prefix must already exist." >&2
    exit 1
  fi
  (
    deps=deps/macos-x86_64
    cmake="cmake -DCMAKE_OSX_ARCHITECTURES=x86_64"
    export deps cmake
    ./libs/build_deps_all.sh
  )
fi
if [ ! -d "$DEPS_PREFIX/lib/cmake/protobuf" ]; then
  echo "ERROR: $DEPS_PREFIX/lib/cmake/protobuf not found after the dependency step." >&2
  exit 1
fi

#### core ####
# build_go.sh (unchanged) writes deployment/macos-amd64/proxor_core and wipes only that dir; the
# x86_64 app is assembled elsewhere.
if [ "${SKIP_GO:-0}" != "1" ]; then
  GOOS=darwin GOARCH=amd64 ./libs/build_go.sh
fi
if [ ! -f "$CORE_DIR/proxor_core" ]; then
  echo "ERROR: $CORE_DIR/proxor_core not found. Build it with GOOS=darwin GOARCH=amd64 ./libs/build_go.sh, or unset SKIP_GO." >&2
  exit 1
fi
if [ "$(lipo -archs "$CORE_DIR/proxor_core")" != "x86_64" ]; then
  echo "ERROR: $CORE_DIR/proxor_core is not x86_64." >&2
  exit 1
fi

#### GUI ####
# Qt 6.7.2 looks for AGL.framework, which newer SDKs no longer ship; point it at OpenGL instead.
SDK="$(xcrun --show-sdk-path 2>/dev/null || true)"
AGL_ARGS=()
if [ -n "$SDK" ] && [ ! -d "$SDK/System/Library/Frameworks/AGL.framework" ]; then
  AGL_ARGS=("-DWrapOpenGL_AGL=$SDK/System/Library/Frameworks/OpenGL.framework")
fi
AVAIL_FLAGS="-Werror=unguarded-availability -Werror=unguarded-availability-new"

# NKR_LIBS is appended to CMAKE_PREFIX_PATH by CMakeLists.txt. CMAKE_MAKE_PROGRAM is explicit
# because CMAKE_IGNORE_PREFIX_PATH would hide a Homebrew ninja in /opt/homebrew or /usr/local.
cmake -S "$SRC_ROOT" -B "$BUILD_DIR" -GNinja -DCMAKE_MAKE_PROGRAM="$(command -v ninja)" \
  -DCMAKE_BUILD_TYPE=Release -DQT_VERSION_MAJOR=6 \
  -DCMAKE_PREFIX_PATH="$PROXOR_QT_DIR" -DNKR_LIBS="$DEPS_PREFIX" \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
  -DCMAKE_IGNORE_PREFIX_PATH="/usr/local;/opt/homebrew" \
  -DCMAKE_CXX_FLAGS="$AVAIL_FLAGS" -DCMAKE_OBJCXX_FLAGS="$AVAIL_FLAGS" \
  ${AGL_ARGS[@]+"${AGL_ARGS[@]}"}
cmake --build "$BUILD_DIR"
if [ ! -f "$BUILD_DIR/Proxor.app/Contents/MacOS/Proxor" ]; then
  echo "ERROR: $BUILD_DIR/Proxor.app/Contents/MacOS/Proxor not found after build." >&2
  exit 1
fi

#### assemble ####
# Start every run from a clean bundle; never deploy or sign the build-tree copy in place.
rm -rf "$APP"
mkdir -p "$DEST"
ditto "$BUILD_DIR/Proxor.app" "$APP"

# In case an older build ever created it.
rm -f "$APP/Contents/MacOS/updater"

# Beside the GUI, because PackageExecutablePath resolves there.
cp "$CORE_DIR/proxor_core" "$APP/Contents/MacOS/"

# Geodata: ConfigBuilder refuses to connect without geoip.db and geosite.db.
if [ ! -f "$DEPLOYMENT/public_res/geosite.db" ]; then
  echo "Downloading geodata..."
  "$SRC_ROOT/libs/build_public_res.sh"
fi
# Seal geodata as resources and keep the lookup path next to the executable alive with relative
# symlinks (data files in Contents/MacOS would be treated as nested code by codesign --deep).
mkdir -p "$APP/Contents/Resources"
for name in geoip.dat geosite.dat geoip.db geosite.db; do
  if [ ! -f "$DEPLOYMENT/public_res/$name" ]; then
    echo "ERROR: Missing geodata asset: $DEPLOYMENT/public_res/$name" >&2
    exit 1
  fi
  cp "$DEPLOYMENT/public_res/$name" "$APP/Contents/Resources/$name"
  chmod 0644 "$APP/Contents/Resources/$name"
  ln -sf "../Resources/$name" "$APP/Contents/MacOS/$name"
done

# Tun/System Proxy service scripts, run once as root through the admin prompt.
mkdir -p "$APP/Contents/Resources/helper"
for name in helper-install.sh helper-uninstall.sh; do
  cp "$SRC_ROOT/packaging/macos/$name" "$APP/Contents/Resources/helper/$name"
  chmod 0755 "$APP/Contents/Resources/helper/$name"
done

#### Qt deployment ####
mkdir -p "$BUILD_DIR"
if ! "$PROXOR_QT_DIR/bin/macdeployqt" "$APP" -verbose=1 2>&1 | tee "$BUILD_DIR/macdeployqt.log"; then
  echo "ERROR: macdeployqt failed." >&2
  exit 1
fi
if grep -q "Cannot resolve rpath" "$BUILD_DIR/macdeployqt.log"; then
  echo "ERROR: macdeployqt logged an unresolved rpath." >&2
  exit 1
fi

# The embedded qt.conf says "Plugins = plugins", resolved against Contents/. On a
# case-sensitive volume Contents/plugins and Contents/PlugIns are different paths.
if [ -d "$APP/Contents/PlugIns" ] && [ ! -e "$APP/Contents/plugins" ]; then
  ln -s PlugIns "$APP/Contents/plugins"
fi

if ! otool -l "$APP/Contents/MacOS/Proxor" | grep -A2 LC_RPATH | grep -q '@executable_path/../Frameworks'; then
  install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/Proxor"
fi

#### thin ####
# Keep only the x86_64 slice of every Mach-O (the official Qt is x86_64 already; this is a guard).
while IFS= read -r -d '' f; do
  case "$(file -b "$f" 2>/dev/null)" in
    Mach-O*) ;;
    *) continue ;;
  esac
  archs="$(lipo -archs "$f")"
  if [ "$archs" != "x86_64" ]; then
    lipo "$f" -thin x86_64 -output "$f.thin"
    mv "$f.thin" "$f"
  fi
done < <(find "$APP" -type f -print0)

#### minimum macOS ####
# Info.plist is sealed by the signature, so this must run before codesign.
plist="$APP/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $MACOSX_DEPLOYMENT_TARGET" "$plist" 2>/dev/null \
  || /usr/libexec/PlistBuddy -c "Add :LSMinimumSystemVersion string $MACOSX_DEPLOYMENT_TARGET" "$plist"

#### sign ####
# Run last: every earlier step modifies the bundle.
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

#### checks ####
test -x "$APP/Contents/MacOS/Proxor"
/usr/bin/plutil -extract NSLocationWhenInUseUsageDescription raw "$APP/Contents/Info.plist" >/dev/null
/usr/bin/plutil -extract NSLocationUsageDescription raw "$APP/Contents/Info.plist" >/dev/null
/usr/bin/plutil -extract NSLocalNetworkUsageDescription raw "$APP/Contents/Info.plist" >/dev/null
test -x "$APP/Contents/MacOS/proxor_core"
test -f "$APP/Contents/MacOS/geosite.db"
test -L "$APP/Contents/MacOS/geosite.db"
test -f "$APP/Contents/Resources/geosite.db"
test -x "$APP/Contents/Resources/helper/helper-install.sh"
test -x "$APP/Contents/Resources/helper/helper-uninstall.sh"

macho_count=0
while IFS= read -r -d '' f; do
  case "$(file -b "$f" 2>/dev/null)" in
    Mach-O*) ;;
    *) continue ;;
  esac
  macho_count=$((macho_count + 1))
  if [ "$(lipo -archs "$f")" != "x86_64" ]; then
    echo "ERROR: not x86_64 only: $f ($(lipo -archs "$f"))" >&2
    exit 1
  fi
  bad_refs="$(otool -L "$f" | tail -n +2 | awk '{print $1}' | grep -E '^(/usr/local/|/opt/homebrew)' || true)"
  if [ -n "$bad_refs" ]; then
    echo "ERROR: $f references Homebrew or /usr/local libraries:" >&2
    echo "$bad_refs" >&2
    exit 1
  fi
done < <(find "$APP" -type f -print0)
echo "INTEL-ARCH-OK: $macho_count Mach-O"

main_minos="$(vtool -arch x86_64 -show-build "$APP/Contents/MacOS/Proxor" | awk '/minos/ {print $2; exit}')"
if [ "$main_minos" != "$MACOSX_DEPLOYMENT_TARGET" ]; then
  echo "ERROR: Proxor minos is '$main_minos', expected $MACOSX_DEPLOYMENT_TARGET" >&2
  exit 1
fi

echo "Proxor.app (x86_64) ready: $APP"
echo "Package it with: PROXOR_MACOS_ARCH=x86_64 MACOSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET ./libs/package_macos.sh"
