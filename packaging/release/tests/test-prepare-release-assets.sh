#!/usr/bin/env bash
set -euo pipefail
script="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)/prepare-release-assets.sh"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT; mkdir -p "$tmp/in/release-assets-source"
printf x > "$tmp/in/release-assets-source/proxor-1.2.3.tar.gz"; sha="$(shasum -a 256 "$tmp/in/release-assets-source/proxor-1.2.3.tar.gz"|awk '{print $1}')"
printf 'version=1.2.3\nsha256=%s\n' "$sha" > "$tmp/in/release-assets-source/proxor-1.2.3.source-manifest"
"$script" prepare-source-release --input "$tmp/in" --version 1.2.3 --output "$tmp/out"; test -f "$tmp/out/proxor-1.2.3.tar.gz"
! "$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final" --public-source-url 'https://invalid.example/proxor-1.2.3.tar.gz'
