#!/usr/bin/env bash
set -euo pipefail

[ "$#" = 2 ] || { printf 'Usage: %s <source-root> <prefix>\n' "$0" >&2; exit 2; }
source_root="$1"
prefix="$2"
cd "$source_root"

# prepare-build-inputs.sh stages .flatpak-input beside the source archive, so the
# directory sits one level above the extracted source root inside flatpak-builder.
build_root="$PWD"
test -d "$build_root/.flatpak-input" || build_root="$(CDPATH= cd -- "$PWD/.." && pwd)"
inputs="$build_root/.flatpak-input"
test -d "$inputs"

# The CI renderer materializes the checksum-verified generated-go-sources.json
# archives in this cache before flatpak-builder executes this script.
test -d "$inputs/go-cache/cache/download"

# The runtime has no Go, so use the toolchain the manifest pinned by checksum and
# forbid any toolchain switch: the build sandbox has no network.
toolchain="$build_root/.flatpak-go-toolchain"
test -x "$toolchain/bin/go"
export PATH="$toolchain/bin:$PATH"
export GOTOOLCHAIN=local
export GOROOT="$toolchain"

export GOPROXY="file://$inputs/go-cache/cache/download"
export GOSUMDB=off
# HOME is not writable in every flatpak-builder sandbox, so keep both Go caches
# inside the build directory. -modcacherw keeps the extracted modules writable so
# flatpak-builder can clean that directory afterwards.
export GOMODCACHE="$build_root/.flatpak-go-modcache"
export GOCACHE="$build_root/.flatpak-go-buildcache"
export GOFLAGS="${GOFLAGS:+$GOFLAGS }-modcacherw"

for asset in geoip.dat geosite.dat geoip.db geosite.db; do
  test -s "$inputs/geodata/$asset"
done
GOOS=linux GOARCH=amd64 ./libs/build_go.sh
# The dependency modules of this manifest installed protobuf, yaml-cpp and
# zxing-cpp into the app prefix, so point the build at that instead of the
# network-fetched libs/deps tree.
cmake -S . -B build -GNinja -DQT_VERSION_MAJOR=6 -DCMAKE_BUILD_TYPE=Release \
  -DNKR_PACKAGE=ON -DNKR_LIBS="$prefix"
cmake --build build

install -Dm755 build/proxor "$prefix/lib/proxor/proxor"
# The embedded qt.conf resolves "Plugins = plugins" against the executable's
# directory, which is what portable builds need. Satisfy it with the runtime's own
# plugin directory instead of shipping a second copy of Qt.
qt_plugins="$(qtpaths6 --query QT_INSTALL_PLUGINS 2>/dev/null \
  || qtpaths --query QT_INSTALL_PLUGINS 2>/dev/null \
  || qmake6 -query QT_INSTALL_PLUGINS)"
test -d "$qt_plugins/platforms"
ln -sfn "$qt_plugins" "$prefix/lib/proxor/plugins"
install -Dm755 deployment/linux64/proxor_core "$prefix/lib/proxor/proxor_core"
install -Dm755 packaging/flatpak/proxor-wrapper.sh "$prefix/bin/proxor"
install -Dm644 assets/linux/io.github.Ogstra.Proxor.desktop "$prefix/share/applications/io.github.Ogstra.Proxor.desktop"
install -Dm644 assets/linux/io.github.Ogstra.Proxor.metainfo.xml "$prefix/share/metainfo/io.github.Ogstra.Proxor.metainfo.xml"
install -Dm644 assets/res/public/proxor.png "$prefix/share/icons/hicolor/256x256/apps/io.github.Ogstra.Proxor.png"
for asset in geoip.dat geosite.dat geoip.db geosite.db; do
  install -Dm644 "$inputs/geodata/$asset" "$prefix/share/proxor/$asset"
done
