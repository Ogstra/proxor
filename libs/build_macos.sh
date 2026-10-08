#!/bin/bash
# Build, assemble, deploy and ad-hoc sign a local Proxor.app for macOS.
#
# The macOS counterpart of deploy_linux.sh, adapted from Throne's
# script/deploy_macos.sh. Layout matters: ProxorGui::PackageExecutablePath()
# resolves proxor_core next to the running GUI binary, i.e.
# Proxor.app/Contents/MacOS/proxor_core, and ConfigBuilder looks for geodata
# in the same directory. The geodata files live in Contents/Resources/ and are
# reached through relative symlinks in Contents/MacOS/ (see the assemble step).
#
# Env knobs: SKIP_GO=1 (reuse an already-built core), BUILD_DIR (override the
# CMake build tree), MACDEPLOYQT (override the macdeployqt binary to use),
# MACOSX_DEPLOYMENT_TARGET (minimum macOS, e.g. 15.0; CI sets it, empty = the
# SDK default).
set -euo pipefail

source libs/env_deploy.sh

#### guards ####
if [ "$(uname -s)" != "Darwin" ]; then
  echo "ERROR: libs/build_macos.sh only runs on macOS." >&2
  exit 1
fi

case "$(uname -m)" in
  arm64) GOARCH=arm64; SUFFIX=macos-arm64 ;;
  x86_64) GOARCH=amd64; SUFFIX=macos-amd64 ;;
  *)
    echo "ERROR: Unsupported macOS architecture: $(uname -m)" >&2
    exit 1
    ;;
esac

for tool in brew cmake ninja go codesign; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required tool not found on PATH: $tool" >&2
    exit 1
  fi
done

MISSING_FORMULAE=()
for formula in qtbase qtsvg protobuf yaml-cpp zxing-cpp; do
  if ! brew list --versions "$formula" >/dev/null 2>&1; then
    MISSING_FORMULAE+=("$formula")
  fi
done
if [ "${#MISSING_FORMULAE[@]}" -gt 0 ]; then
  echo "ERROR: missing Homebrew dependencies. Install them first:" >&2
  echo "  brew install ${MISSING_FORMULAE[*]}" >&2
  exit 1
fi

BUILD_DIR="${BUILD_DIR:-$SRC_ROOT/build-macos}"
DEST="$DEPLOYMENT/$SUFFIX"
APP="$DEST/Proxor.app"
BREW_PREFIX="$(brew --prefix)"

#### core ####
# build_go.sh wipes $DEST on every run, so it must run before the bundle is
# copied in (the assemble step below copies from $BUILD_DIR into $DEST/Proxor.app).
if [ "${SKIP_GO:-0}" != "1" ]; then
  GOOS=darwin GOARCH=$GOARCH ./libs/build_go.sh
fi
if [ ! -f "$DEST/proxor_core" ]; then
  echo "ERROR: $DEST/proxor_core not found. Build it with GOOS=darwin GOARCH=$GOARCH ./libs/build_go.sh, or unset SKIP_GO." >&2
  exit 1
fi

#### GUI ####
# NKR_DISABLE_LIBS is mandatory: libs/deps/built is the Windows prefix (Windows
# .lib files and protobuf 21.4 headers), and it would shadow Homebrew if left on
# CMAKE_PREFIX_PATH.
cmake -S "$SRC_ROOT" -B "$BUILD_DIR" -GNinja \
  -DCMAKE_BUILD_TYPE=Release -DQT_VERSION_MAJOR=6 \
  -DNKR_DISABLE_LIBS=ON -DCMAKE_PREFIX_PATH="$BREW_PREFIX" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-}"
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
cp "$DEST/proxor_core" "$APP/Contents/MacOS/"

# Geodata: ConfigBuilder refuses to connect without geoip.db and geosite.db.
if [ ! -f "$DEPLOYMENT/public_res/geosite.db" ]; then
  echo "Downloading geodata..."
  "$SRC_ROOT/libs/build_public_res.sh"
fi
# Data files placed in Contents/MacOS are treated as nested code by `codesign --deep`, and
# their signatures live in xattrs that zip/unzip (Homebrew's extraction) drop. Seal them as
# resources and keep the lookup path next to the executable alive with relative symlinks.
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

# Tun/System Proxy service scripts, run once as root through the admin prompt (plan 50-07).
mkdir -p "$APP/Contents/Resources/helper"
for name in helper-install.sh helper-uninstall.sh; do
  cp "$SRC_ROOT/packaging/macos/$name" "$APP/Contents/Resources/helper/$name"
  chmod 0755 "$APP/Contents/Resources/helper/$name"
done

#### Qt deployment ####
MACDEPLOYQT="${MACDEPLOYQT:-}"
if [ -z "$MACDEPLOYQT" ]; then
  if command -v macdeployqt >/dev/null 2>&1; then
    MACDEPLOYQT="$(command -v macdeployqt)"
  elif [ -x "$BREW_PREFIX/opt/qtbase/bin/macdeployqt" ]; then
    MACDEPLOYQT="$BREW_PREFIX/opt/qtbase/bin/macdeployqt"
  elif [ -x "$BREW_PREFIX/share/qt/libexec/macdeployqt" ]; then
    MACDEPLOYQT="$BREW_PREFIX/share/qt/libexec/macdeployqt"
  else
    MACDEPLOYQT="$(find "$BREW_PREFIX" -name macdeployqt -type f 2>/dev/null | head -1 || true)"
  fi
fi

USED_PLUGIN_FALLBACK=0
if [ -n "$MACDEPLOYQT" ] && [ -x "$MACDEPLOYQT" ]; then
  if "$MACDEPLOYQT" "$APP" -verbose=1 2>&1 | tee "$BUILD_DIR/macdeployqt.log"; then
    if grep -q "Cannot resolve rpath" "$BUILD_DIR/macdeployqt.log"; then
      echo "WARNING: macdeployqt logged an unresolved rpath; falling back to the plugin symlink." >&2
      USED_PLUGIN_FALLBACK=1
    fi
  else
    echo "WARNING: macdeployqt failed; falling back to the plugin symlink." >&2
    USED_PLUGIN_FALLBACK=1
  fi
else
  echo "WARNING: macdeployqt not found; falling back to the plugin symlink." >&2
  USED_PLUGIN_FALLBACK=1
fi

if [ "$USED_PLUGIN_FALLBACK" = "1" ]; then
  ln -sfn "$BREW_PREFIX/share/qt/plugins" "$APP/Contents/plugins"
  echo "WARNING: Proxor.app now depends on the installed Homebrew qtbase/qtsvg kegs (no macdeployqt bundling)." >&2
fi

# The embedded qt.conf says "Plugins = plugins", resolved against Contents/. On a
# case-sensitive volume Contents/plugins and Contents/PlugIns are different paths.
if [ -d "$APP/Contents/PlugIns" ] && [ ! -e "$APP/Contents/plugins" ]; then
  ln -s PlugIns "$APP/Contents/plugins"
fi

#### fix up Homebrew-absolute references macdeployqt missed ####
# macdeployqt does not always rewrite every transitive dependency (observed: brotli,
# pulled in indirectly through freetype/harfbuzz for WOFF2 fonts). It also does not
# add an @executable_path rpath to the main executable in every case, which leaves
# @rpath-relative loads (like libbrotlidec -> libbrotlicommon) resolving through the
# Homebrew prefix instead of the bundle. Rewrite both, so the bundle stays
# self-contained instead of depending on the installed Homebrew kegs.
if [ "$USED_PLUGIN_FALLBACK" != "1" ]; then
  if ! otool -l "$APP/Contents/MacOS/Proxor" | grep -A2 LC_RPATH | grep -q '@executable_path/../Frameworks'; then
    install_name_tool -add_rpath "@executable_path/../Frameworks" "$APP/Contents/MacOS/Proxor"
  fi

  while IFS= read -r -d '' candidate; do
    case "$(file -b "$candidate" 2>/dev/null)" in
      Mach-O*) ;;
      *) continue ;;
    esac
    id_line=$(otool -D "$candidate" 2>/dev/null | tail -1)
    case "$id_line" in
      "$BREW_PREFIX"/*|/opt/homebrew/*)
        install_name_tool -id "@rpath/$(basename "$candidate")" "$candidate"
        ;;
    esac
    while IFS= read -r dep; do
      case "$dep" in
        "$BREW_PREFIX"/*|/opt/homebrew/*)
          install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$candidate"
          ;;
      esac
    done < <(otool -L "$candidate" 2>/dev/null | tail -n +2 | awk '{print $1}')
  done < <(find "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" -type f -print0 2>/dev/null)
fi

#### minimum macOS ####
# The CMake default Info.plist template may leave LSMinimumSystemVersion empty. Info.plist is
# sealed by the signature, so this must run before codesign.
if [ -n "${MACOSX_DEPLOYMENT_TARGET:-}" ]; then
  plist="$APP/Contents/Info.plist"
  /usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $MACOSX_DEPLOYMENT_TARGET" "$plist" 2>/dev/null \
    || /usr/libexec/PlistBuddy -c "Add :LSMinimumSystemVersion string $MACOSX_DEPLOYMENT_TARGET" "$plist"
fi

#### sign ####
# Run last: every earlier step modifies the bundle, and arm64 requires a valid
# signature on every Mach-O it contains.
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

if [ "$USED_PLUGIN_FALLBACK" != "1" ]; then
  BAD_REFS=$(otool -L "$APP/Contents/MacOS/Proxor" | grep -E "/opt/homebrew|$BREW_PREFIX" || true)
  if [ -d "$APP/Contents/Frameworks" ]; then
    for dylib in "$APP"/Contents/Frameworks/*.dylib; do
      [ -e "$dylib" ] || continue
      DYLIB_REFS=$(otool -L "$dylib" | grep -E "/opt/homebrew|$BREW_PREFIX" || true)
      if [ -n "$DYLIB_REFS" ]; then
        BAD_REFS="$BAD_REFS
$dylib:
$DYLIB_REFS"
      fi
    done
  fi
  if [ -n "$BAD_REFS" ]; then
    echo "ERROR: Homebrew-relative references found after macdeployqt:" >&2
    echo "$BAD_REFS" >&2
    exit 1
  fi
else
  echo "Skipping the Homebrew-reference check: the plugin symlink fallback was used." >&2
fi

echo "Proxor.app ready: $APP"
echo "Run it with: open \"$APP\""
echo "If this bundle is ever copied through a download, clear quarantine first:"
echo "  xattr -dr com.apple.quarantine \"$APP\""
