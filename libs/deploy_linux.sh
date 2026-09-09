#!/bin/bash
# Assemble an AppDir for the Linux AppImage.
#
# The Linux counterpart of deploy_windows64.sh, which had no equivalent: docs/Build_Linux.md
# stated plainly that no supported Linux packaging existed.
#
# Layout matters. ProxorGui::PackageExecutablePath() resolves relative to the running
# binary's directory, so proxor_core must sit next to proxor. Geodata goes to
# usr/share/proxor, which FindCoreAsset() already searches.
set -e

source libs/env_deploy.sh

APPDIR="${APPDIR:-$DEPLOYMENT/AppDir}"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" "$APPDIR/usr/share/proxor"

#### GUI ####
if [ ! -f "$BUILD/proxor" ]; then
  echo "ERROR: $BUILD/proxor not found. Build the GUI first." >&2
  exit 1
fi
cp "$BUILD/proxor" "$APPDIR/usr/bin/"

#### core + updater ####
# Placed beside the GUI so PackageExecutablePath("proxor_core") resolves.
for bin in proxor_core launcher updater; do
  for src in "$DEPLOYMENT/linux64/$bin" "$SRC_ROOT/go/cmd/proxor_core/$bin" "$SRC_ROOT/go/cmd/updater/$bin"; do
    if [ -f "$src" ]; then
      cp "$src" "$APPDIR/usr/bin/"
      break
    fi
  done
done

if [ ! -f "$APPDIR/usr/bin/proxor_core" ]; then
  echo "ERROR: proxor_core not found; build it with GOOS=linux GOARCH=amd64 ./libs/build_go.sh" >&2
  exit 1
fi

#### desktop entry + icon ####
cp "$SRC_ROOT/assets/linux/proxor.desktop" "$APPDIR/usr/share/applications/"
cp "$SRC_ROOT/assets/res/public/proxor.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/"
# linuxdeploy also wants them at the AppDir root.
cp "$SRC_ROOT/assets/linux/proxor.desktop" "$APPDIR/"
cp "$SRC_ROOT/assets/res/public/proxor.png" "$APPDIR/"

#### geodata ####
if [ -d "$DEPLOYMENT/public_res" ]; then
  for name in geoip.dat geosite.dat geoip.db geosite.db; do
    [ -f "$DEPLOYMENT/public_res/$name" ] && cp "$DEPLOYMENT/public_res/$name" "$APPDIR/usr/share/proxor/"
  done
fi

#### AppRun hook ####
# linuxdeploy-plugin-qt logs "skipping AppRun hook creation on Qt 6" and relies on
# usr/bin/qt.conf alone. When that does not take effect the plugin search path ends up
# empty and Qt aborts with:
#   Could not find the Qt platform plugin "xcb" in ""
# The plugin binaries are present in the AppImage; only the paths are missing. Set them
# explicitly. linuxdeploy sources every file in apprun-hooks/ from its AppRun.
mkdir -p "$APPDIR/apprun-hooks"
cat > "$APPDIR/apprun-hooks/proxor-qt-paths.sh" <<'HOOK'
#!/bin/sh
export QT_PLUGIN_PATH="${APPDIR}/usr/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export QT_QPA_PLATFORM_PLUGIN_PATH="${APPDIR}/usr/plugins/platforms"
HOOK
chmod +x "$APPDIR/apprun-hooks/proxor-qt-paths.sh"

echo "AppDir ready: $APPDIR"
ls -la "$APPDIR/usr/bin"
