#!/usr/bin/env bash
set -euo pipefail
usage() { echo "Usage: $0 --version X.Y.Z --sha256 HASH [--sha256-intel HASH] --output DIR" >&2; exit 2; }
hi=""
if [ "$#" = 6 ]; then
  [ "$1" = --version ] && [ "$3" = --sha256 ] && [ "$5" = --output ] || usage
  v="$2"; h="$4"; out="$6"
elif [ "$#" = 8 ]; then
  [ "$1" = --version ] && [ "$3" = --sha256 ] && [ "$5" = --sha256-intel ] && [ "$7" = --output ] || usage
  v="$2"; h="$4"; hi="$6"; out="$8"
else
  usage
fi
printf '%s' "$v" | grep -Eqx '[0-9]+\.[0-9]+\.[0-9]+' || { echo "invalid version: $v" >&2; exit 1; }
printf '%s' "$h" | grep -Eqx '[0-9a-f]{64}' || { echo "invalid sha256: $h" >&2; exit 1; }
if [ -n "$hi" ]; then
  printf '%s' "$hi" | grep -Eqx '[0-9a-f]{64}' || { echo "invalid intel sha256: $hi" >&2; exit 1; }
  [ "$hi" != "$h" ] || { echo "the Intel and Apple Silicon sha256 must differ" >&2; exit 1; }
fi
root="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
mkdir -p "$out"
if [ -n "$hi" ]; then
  sed -e "s|@VERSION@|$v|g" -e "s|@SHA256@|$h|g" -e "s|@SHA256_INTEL@|$hi|g" "$root/proxor-two-arch.rb.in" > "$out/proxor.rb.tmp"
else
  sed -e "s|@VERSION@|$v|g" -e "s|@SHA256@|$h|g" "$root/proxor.rb.in" > "$out/proxor.rb.tmp"
fi
if grep -Eq '@[A-Z0-9_]+@' "$out/proxor.rb.tmp"; then
  rm -f "$out/proxor.rb.tmp"; echo "unreplaced placeholder in the cask template" >&2; exit 1
fi
mv "$out/proxor.rb.tmp" "$out/proxor.rb"
