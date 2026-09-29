#!/usr/bin/env bash
set -euo pipefail
usage() { echo "Usage: $0 --version X.Y.Z --sha256 HASH --output DIR" >&2; exit 2; }
[ "$#" = 6 ] || usage
[ "$1" = --version ] && [ "$3" = --sha256 ] && [ "$5" = --output ] || usage
v="$2"; h="$4"; out="$6"
printf '%s' "$v" | grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+' || { echo "invalid version: $v" >&2; exit 1; }
printf '%s' "$h" | grep -Eqx '[0-9a-f]{64}' || { echo "invalid sha256: $h" >&2; exit 1; }
root="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
mkdir -p "$out"
sed -e "s|@VERSION@|$v|g" -e "s|@SHA256@|$h|g" "$root/proxor.rb.in" > "$out/proxor.rb.tmp"
if grep -Eq '@[A-Z0-9_]+@' "$out/proxor.rb.tmp"; then
  rm -f "$out/proxor.rb.tmp"; echo "unreplaced placeholder in the cask template" >&2; exit 1
fi
mv "$out/proxor.rb.tmp" "$out/proxor.rb"
