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

PACKAGE_ROOT="$STAGE_DIR/nekoray"
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
    config|*.log|*.dmp)
      continue
      ;;
  esac
  cp -R "$path" "$PACKAGE_ROOT/"
done

rm -f "$OUTPUT_ZIP_ABS"
if command -v zip >/dev/null 2>&1; then
  ( cd "$STAGE_DIR" && zip -r "$OUTPUT_ZIP_ABS" nekoray >/dev/null )
else
  PACKAGE_ROOT_WIN="$(to_windows_path "$PACKAGE_ROOT")"
  OUTPUT_ZIP_WIN="$(to_windows_path "$OUTPUT_ZIP_ABS")"
  powershell.exe -NoProfile -Command "Compress-Archive -Path '$PACKAGE_ROOT_WIN\\*' -DestinationPath '$OUTPUT_ZIP_WIN' -Force"
fi
