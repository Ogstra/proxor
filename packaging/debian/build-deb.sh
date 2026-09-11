#!/usr/bin/env bash
set -euo pipefail

usage() { printf 'Usage: %s --source-root <proxor-X.Y.Z>\n' "$0" >&2; exit 2; }
[ "$#" = 2 ] && [ "$1" = --source-root ] || usage
root="$2"
[ -d "$root/.git" ] && { printf 'source root must be a staged archive, not a checkout\n' >&2; exit 1; }
[ -f "$root/VERSION.txt" ] && [ -f "$root/packaging/debian/debian/rules" ] || usage
version="$(tr -d '\n' < "$root/VERSION.txt")"
case "$version" in [0-9]*.[0-9]*.[0-9]*) ;; *) printf 'invalid version: %s\n' "$version" >&2; exit 1;; esac
base="$(basename "$root")"
[ "$base" = "proxor-$version" ] || { printf 'source root/version mismatch\n' >&2; exit 1; }

rm -rf "$root/debian"
cp -a "$root/packaging/debian/debian" "$root/debian"
date_rfc2822="$(LC_ALL=C date -Ru)"
python3 - "$root/debian/changelog" "$version" "$date_rfc2822" <<'PY'
from pathlib import Path
import sys
p = Path(sys.argv[1])
p.write_text(p.read_text().replace('@VERSION@', sys.argv[2]).replace('@DATE@', sys.argv[3]))
PY
(cd "$root" && dpkg-buildpackage -us -uc -b)
