#!/usr/bin/env bash
set -euo pipefail
script="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)/prepare-release-assets.sh"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT; mkdir -p "$tmp/in/release-assets-source"
printf x > "$tmp/in/release-assets-source/proxor-1.2.3.tar.gz"; sha="$(shasum -a 256 "$tmp/in/release-assets-source/proxor-1.2.3.tar.gz"|awk '{print $1}')"
printf 'version=1.2.3\nsha256=%s\n' "$sha" > "$tmp/in/release-assets-source/proxor-1.2.3.source-manifest"
"$script" prepare-source-release --input "$tmp/in" --version 1.2.3 --output "$tmp/out"; test -f "$tmp/out/proxor-1.2.3.tar.gz"
! "$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final" --public-source-url 'https://invalid.example/proxor-1.2.3.tar.gz'

url="https://github.com/Ogstra/proxor/releases/download/v1.2.3/proxor-1.2.3.tar.gz"
mkdir -p "$tmp/in/release-assets-arch"
printf 'source=("proxor-1.2.3.tar.gz::%s")\nsha256sums=(%s)\n' "$url" "$sha" > "$tmp/in/release-assets-arch/PKGBUILD"
printf '\tsource = proxor-1.2.3.tar.gz::%s\n\tsha256sums = %s\n' "$url" "$sha" > "$tmp/in/release-assets-arch/.SRCINFO"
for asset in proxor-1.2.3-windows64.zip proxor-1.2.3-winget-x64.zip proxor-1.2.3.AppImage \
  proxor-1.2.3.deb proxor-1.2.3.rpm proxor-1.2.3.flatpak; do
  mkdir -p "$tmp/in/$asset.d"; printf '%s' "$asset" > "$tmp/in/$asset.d/$asset"
done
# Per-job build artifacts share names across jobs and never become release assets.
for job in linux windows; do
  mkdir -p "$tmp/in/$job"; printf x > "$tmp/in/$job/artifacts.tgz"
done
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final" --public-source-url "$url"
test -f "$tmp/final/proxor-1.2.3.flatpak"
test ! -e "$tmp/final/artifacts.tgz"

# Two release assets with the same name would silently overwrite each other.
mkdir -p "$tmp/in/duplicate"; printf y > "$tmp/in/duplicate/proxor-1.2.3.deb"
! "$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-duplicate" --public-source-url "$url"
