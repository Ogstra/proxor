#!/bin/bash
set -e

source libs/env_deploy.sh
DEST=$DEPLOYMENT/windows64
rm -rf $DEST
mkdir -p $DEST

#### copy exe ####
cp $BUILD/proxor.exe $DEST
mkdir -p $DEST/config/runtime
cp $BUILD/app.exe $DEST/config/runtime/
cp $SRC_ROOT/go/cmd/proxor_core/proxor_core.exe $DEST
cp $SRC_ROOT/go/cmd/updater/updater.exe $DEST

#### deploy qt & DLL runtime ####
pushd $DEST/config/runtime
windeployqt app.exe --no-system-d3d-compiler --no-opengl-sw --verbose 2
rm -rf translations
rm -rf libEGL.dll libGLESv2.dll Qt6Pdf.dll
rm -f dxcompiler.dll dxil.dll

if [ "$DL_QT_VER" != "5.15" ]; then
  cp $SRC_ROOT/qtsdk/Qt/bin/libcrypto-3-x64.dll .
  cp $SRC_ROOT/qtsdk/Qt/bin/libssl-3-x64.dll .
fi

MSVC_REDIST_DIR=""
if [ -n "$VCToolsRedistDir" ] && [ -d "${VCToolsRedistDir}/x64/Microsoft.VC143.CRT" ]; then
  MSVC_REDIST_DIR="${VCToolsRedistDir}/x64/Microsoft.VC143.CRT"
else
  for candidate in \
    "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/14.44.35112/x64/Microsoft.VC143.CRT" \
    "/c/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Redist/MSVC/v143/x64/Microsoft.VC143.CRT"
  do
    if [ -d "$candidate" ]; then
      MSVC_REDIST_DIR="$candidate"
      break
    fi
  done
fi

if [ -n "$MSVC_REDIST_DIR" ]; then
  cp "$MSVC_REDIST_DIR"/*.dll .
  rm -f vc_redist.x64.exe
fi

popd

#### move required Qt plugins under config/runtime/plugins ####
PLUGIN_DEST="$DEST/config/runtime/plugins"
mkdir -p "$PLUGIN_DEST/platforms"
mkdir -p "$PLUGIN_DEST/styles"
mkdir -p "$PLUGIN_DEST/tls"
mkdir -p "$PLUGIN_DEST/iconengines"
mkdir -p "$PLUGIN_DEST/imageformats"

cp "$DEST/config/runtime/platforms/qwindows.dll" "$PLUGIN_DEST/platforms/"
cp "$DEST/config/runtime/styles/qmodernwindowsstyle.dll" "$PLUGIN_DEST/styles/"
cp "$DEST/config/runtime/tls/qcertonlybackend.dll" "$PLUGIN_DEST/tls/"
cp "$DEST/config/runtime/tls/qopensslbackend.dll" "$PLUGIN_DEST/tls/"
cp "$DEST/config/runtime/tls/qschannelbackend.dll" "$PLUGIN_DEST/tls/"
cp "$DEST/config/runtime/iconengines/qsvgicon.dll" "$PLUGIN_DEST/iconengines/"
cp "$DEST/config/runtime/imageformats/qsvg.dll" "$PLUGIN_DEST/imageformats/"

rm -rf "$DEST/config/runtime/generic" "$DEST/config/runtime/iconengines" "$DEST/config/runtime/imageformats" "$DEST/config/runtime/networkinformation" \
       "$DEST/config/runtime/platforms" "$DEST/config/runtime/sqldrivers" "$DEST/config/runtime/styles" "$DEST/config/runtime/tls"

#### prepare deployment ####
if [ -d "$DEPLOYMENT/public_res" ]; then
  mkdir -p "$DEST/config"
  for name in geoip.dat geosite.dat geoip.db geosite.db; do
    if [ -f "$DEPLOYMENT/public_res/$name" ]; then
      cp "$DEPLOYMENT/public_res/$name" "$DEST/config/"
    fi
  done
fi

cp $BUILD/*.pdb $DEPLOYMENT
