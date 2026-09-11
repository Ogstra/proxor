#!/usr/bin/env bash
set -euo pipefail
[ "$1" = --archive ] && [ "$3" = --workdir ] && [ "$5" = --owner ] || exit 2
archive="$2"; work="$4"; owner="$6"; [ -f "$archive" ] || exit 1
case "$archive" in *"/checkout/"*) exit 1;; esac
rm -rf "$work"; mkdir -p "$work"; tar -xzf "$archive" -C "$work"
root="$(find "$work" -mindepth 1 -maxdepth 1 -type d -name 'proxor-*' -print -quit)"; [ -n "$root" ]
test -d "$root/3rdparty/sing-box"; test -d "$root/3rdparty/QHotkey"; test -d "$root/3rdparty/SQLiteCpp"; ! find "$root" -name .git -print -quit | grep -q .
chown -R "$owner" "$root"
