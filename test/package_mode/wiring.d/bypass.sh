#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
cb="$repo_root/src/db/ConfigBuilder.cpp"
ab="$repo_root/src/platform/AutoBypass.cpp"

for pat in \
  'ProxorPlatform::BuildAutoBypassProcesses(externalCorePrograms(status->result)' \
  '{"process_path", QList2QJsonArray(autoBypass.processPaths)}' \
  'ProxorPlatform::KnownVpnClientProcessNames(ProxorPlatform::CompiledHostOs())'; do
  grep -qF "$pat" "$cb" || { echo "bypass.sh: ConfigBuilder.cpp missing: $pat" >&2; exit 1; }
done
for lit in wireguard.exe openvpn.exe tailscaled.exe; do
  grep -qF "\"$lit\"" "$ab" || { echo "bypass.sh: AutoBypass.cpp missing $lit" >&2; exit 1; }
done
if grep -nE 'getAutoBypassExternalProcessPaths|"tailscaled.exe"' "$cb"; then
  echo "bypass.sh: the VPN client list lives in AutoBypass.cpp only" >&2
  exit 1
fi
