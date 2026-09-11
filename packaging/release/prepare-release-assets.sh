#!/usr/bin/env bash
set -euo pipefail
usage() { echo "Usage: $0 prepare-source-release|prepare-final-assets --input DIR --version X.Y.Z --output DIR [--public-source-url URL]" >&2; exit 2; }
phase="${1:-}"; shift || true
input= output= version= public_url=
while [ "$#" -gt 0 ]; do case "$1" in --input) input="$2"; shift 2;; --output) output="$2"; shift 2;; --version) version="$2"; shift 2;; --public-source-url) public_url="$2"; shift 2;; *) usage;; esac; done
case "$version" in [0-9]*.[0-9]*.[0-9]*) ;; *) usage;; esac; [ -d "$input" ] && [ -n "$output" ] || usage
source="$(find "$input" -type f -name "proxor-$version.tar.gz" -print -quit)"; manifest="$(find "$input" -type f -name "proxor-$version.source-manifest" -print -quit)"
[ -n "$source" ] && [ -n "$manifest" ] || { echo 'verified source artifact is required' >&2; exit 1; }
sha="$(shasum -a 256 "$source" | awk '{print $1}')"; grep -qx "version=$version" "$manifest"; grep -qx "sha256=$sha" "$manifest"
mkdir -p "$output"; cp "$source" "$manifest" "$output/"
if [ "$phase" = prepare-source-release ]; then exit 0; fi
[ "$phase" = prepare-final-assets ] || usage
case "$public_url" in "https://github.com/Ogstra/proxor/releases/download/"*/"proxor-$version.tar.gz") ;; *) echo 'public source URL must be the attached release asset' >&2; exit 1;; esac
for required in '*-windows64.zip' '*-winget-x64.zip' '*.AppImage' '*.deb' '*.rpm' '*.flatpak'; do find "$input" -type f -name "$required" -print -quit | grep -q . || { echo "missing $required" >&2; exit 1; }; done
! find "$input" -type f \( -iname '*.msi' -o -iname '*.dmg' \) -print -quit | grep -q .
winget="$(find "$input" -type f -name '*-winget-x64.zip' -print -quit)"; winget_sha="$(shasum -a 256 "$winget"|awk '{print $1}')"
packaging/winget/render-manifest.sh --version "$version" --url "https://github.com/Ogstra/proxor/releases/download/${public_url##*/downloads/}" --sha256 "$winget_sha" --output "$output/winget-manifests"
packaging/arch/render-pkgbuild.sh --version "$version" --url "$public_url" --sha256 "$sha" --output "$output/aur"
find "$input" -type f \( -name '*.zip' -o -name '*.AppImage' -o -name '*.deb' -o -name '*.rpm' -o -name '*.flatpak' \) -exec cp {} "$output/" \;
(cd "$output" && find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 shasum -a 256 > SHA256SUMS && shasum -a 256 -c SHA256SUMS)
