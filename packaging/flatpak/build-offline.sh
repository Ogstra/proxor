#!/usr/bin/env bash
set -euo pipefail

[ "$#" = 2 ] || { printf 'Usage: %s <source-root> <prefix>\n' "$0" >&2; exit 2; }
source_root="$1"
prefix="$2"
cd "$source_root"

# The CI renderer materializes the checksum-verified generated-go-sources.json
# archives in this cache before flatpak-builder executes this script.
test -d .flatpak-go-cache/cache/download
export GOPROXY="file://$PWD/.flatpak-go-cache"
export GOSUMDB=off

for asset in geoip.dat geosite.dat geoip.db geosite.db; do
  test -s ".flatpak-input/geodata/$asset"
done
GOOS=linux GOARCH=amd64 ./libs/build_go.sh
cmake -S . -B build -GNinja -DQT_VERSION_MAJOR=6 -DCMAKE_BUILD_TYPE=Release -DNKR_PACKAGE=ON
cmake --build build

install -Dm755 build/proxor "$prefix/lib/proxor/proxor"
install -Dm755 deployment/linux64/proxor_core "$prefix/lib/proxor/proxor_core"
install -Dm755 packaging/flatpak/proxor-wrapper.sh "$prefix/bin/proxor"
install -Dm644 assets/linux/io.github.Ogstra.Proxor.desktop "$prefix/share/applications/io.github.Ogstra.Proxor.desktop"
install -Dm644 assets/linux/io.github.Ogstra.Proxor.metainfo.xml "$prefix/share/metainfo/io.github.Ogstra.Proxor.metainfo.xml"
install -Dm644 assets/res/public/proxor.png "$prefix/share/icons/hicolor/256x256/apps/io.github.Ogstra.Proxor.png"
for asset in geoip.dat geosite.dat geoip.db geosite.db; do
  install -Dm644 ".flatpak-input/geodata/$asset" "$prefix/share/proxor/$asset"
done
