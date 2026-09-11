#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 8 ] || { echo "Usage: $0 --version V --url URL --sha256 HASH --output DIR" >&2; exit 2; }
[ "$1" = --version ] && [ "$3" = --url ] && [ "$5" = --sha256 ] && [ "$7" = --output ] || exit 2
v="$2"; u="$4"; h="$6"; out="$8"; case "$v:$h" in [0-9]*.[0-9]*.[0-9]*:[0-9a-f][0-9a-f]*) ;; *) exit 1;; esac
case "$u" in https://github.com/Ogstra/proxor/releases/download/*/proxor-"$v".tar.gz) ;; *) exit 1;; esac
mkdir -p "$out"; sed -e "s|@VERSION@|$v|g" -e "s|@SOURCE_URL@|$u|g" -e "s|@SHA256@|$h|g" packaging/arch/PKGBUILD.in > "$out/PKGBUILD"; (cd "$out" && makepkg --printsrcinfo > .SRCINFO)
