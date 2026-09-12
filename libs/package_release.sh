#!/bin/bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <source-dir> <output-zip>"
  exit 1
fi

SOURCE_DIR="$1"
OUTPUT_ZIP="$2"

if [ ! -d "$SOURCE_DIR" ]; then
  echo "Source directory not found: $SOURCE_DIR"
  exit 1
fi

OUTPUT_DIR="$(dirname "$OUTPUT_ZIP")"
OUTPUT_NAME="$(basename "$OUTPUT_ZIP")"
mkdir -p "$OUTPUT_DIR"
OUTPUT_DIR_ABS="$(cd "$OUTPUT_DIR" && pwd)"
OUTPUT_ZIP_ABS="$OUTPUT_DIR_ABS/$OUTPUT_NAME"

STAGE_DIR="$(mktemp -d "${PWD}/.tmp-release.XXXXXX")"
trap 'rm -rf "$STAGE_DIR"' EXIT

PACKAGE_ROOT="$STAGE_DIR/proxor"
mkdir -p "$PACKAGE_ROOT"

to_windows_path() {
  local path="$1"
  if command -v cygpath >/dev/null 2>&1; then
    cygpath -w "$path"
    return
  fi
  case "$path" in
    /mnt/[a-zA-Z]/*)
      local drive="${path:5:1}"
      local rest="${path:7}"
      rest="${rest//\//\\}"
      printf '%s:\\%s\n' "${drive^^}" "$rest"
      ;;
    /[a-zA-Z]/*)
      local drive="${path:1:1}"
      local rest="${path:3}"
      rest="${rest//\//\\}"
      printf '%s:\\%s\n' "${drive^^}" "$rest"
      ;;
    *)
      printf '%s\n' "$path"
      ;;
  esac
}

shopt -s dotglob nullglob
for path in "$SOURCE_DIR"/*; do
  name="$(basename "$path")"
  case "$name" in
    *.log|*.dmp)
      continue
      ;;
  esac
  cp -R "$path" "$PACKAGE_ROOT/"
done

rm -f "$OUTPUT_ZIP_ABS"
# A ZIP stores paths with forward slashes. Windows PowerShell's Compress-Archive and the
# .NET Framework ZipFile both write backslashes instead, which turns every directory into
# part of a file name, so those are only used through pwsh (.NET Core) and the result is
# verified below rather than trusted.
if command -v zip >/dev/null 2>&1; then
  ( cd "$STAGE_DIR" && zip -r "$OUTPUT_ZIP_ABS" proxor >/dev/null )
elif command -v 7z >/dev/null 2>&1; then
  ( cd "$STAGE_DIR" && 7z a -tzip -bso0 -bsp0 "$(to_windows_path "$OUTPUT_ZIP_ABS")" proxor >/dev/null )
elif command -v pwsh >/dev/null 2>&1; then
  pwsh -NoProfile -Command "Compress-Archive -Path '$(to_windows_path "$PACKAGE_ROOT")' -DestinationPath '$(to_windows_path "$OUTPUT_ZIP_ABS")' -Force"
else
  printf '%s\n' 'no archiver that writes ZIP paths with forward slashes is available (zip, 7z or pwsh)' >&2
  exit 1
fi

entries="$(
  if command -v unzip >/dev/null 2>&1; then
    unzip -Z1 "$OUTPUT_ZIP_ABS"
  else
    python3 -c 'import sys, zipfile; print("\n".join(zipfile.ZipFile(sys.argv[1]).namelist()))' "$OUTPUT_ZIP_ABS"
  fi
)"
if grep -q '\\' <<<"$entries"; then
  printf '%s\n' "$OUTPUT_ZIP_ABS stores backslash separators, so its directories are file names" >&2
  exit 1
fi
grep -q '^proxor/' <<<"$entries" || { printf '%s\n' "$OUTPUT_ZIP_ABS has no proxor/ root" >&2; exit 1; }
