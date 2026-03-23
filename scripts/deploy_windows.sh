#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SCRIPT_PATH="$SCRIPT_DIR/deploy_windows.ps1"

if command -v pwsh.exe >/dev/null 2>&1; then
  POWERSHELL_BIN="pwsh.exe"
elif command -v powershell.exe >/dev/null 2>&1; then
  POWERSHELL_BIN="powershell.exe"
else
  echo "PowerShell was not found in PATH." >&2
  exit 1
fi

if command -v wslpath >/dev/null 2>&1; then
  SCRIPT_PATH="$(wslpath -w "$SCRIPT_PATH")"
fi

"$POWERSHELL_BIN" -NoProfile -ExecutionPolicy Bypass -File "$SCRIPT_PATH" "$@"
