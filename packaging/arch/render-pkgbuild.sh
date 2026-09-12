#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 8 ] || [ "$#" = 9 ] || { echo "Usage: $0 --version V --url URL --sha256 HASH --output DIR [--pkgbuild-only]" >&2; exit 2; }
[ "$1" = --version ] && [ "$3" = --url ] && [ "$5" = --sha256 ] && [ "$7" = --output ] || exit 2
# .SRCINFO is only consumed by the AUR; building locally needs the PKGBUILD alone, and
# makepkg that writes it exists on Arch only.
srcinfo=true; [ "${9:-}" != --pkgbuild-only ] || srcinfo=false
[ "$#" = 8 ] || [ "${9:-}" = --pkgbuild-only ] || exit 2
v="$2"; u="$4"; h="$6"; out="$8"; case "$v:$h" in [0-9]*.[0-9]*.[0-9]*:[0-9a-f][0-9a-f]*) ;; *) exit 1;; esac
case "$u" in https://github.com/Ogstra/proxor/releases/download/*/proxor-"$v".tar.gz|file:///checkout/source-input/proxor-"$v".tar.gz) ;; *) exit 1;; esac
root="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
mkdir -p "$out"; sed -e "s|@VERSION@|$v|g" -e "s|@SOURCE_URL@|$u|g" -e "s|@SHA256@|$h|g" "$root/PKGBUILD.in" > "$out/PKGBUILD"; if "$srcinfo"; then (cd "$out" && makepkg --printsrcinfo > .SRCINFO); fi
