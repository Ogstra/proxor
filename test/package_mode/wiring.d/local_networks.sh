#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "local_networks.sh: $*" >&2; exit 1; }
cfg="$repo_root/src/db/ConfigBuilder.cpp"
for blanket in '"10.0.0.0/8"' '"172.16.0.0/12"' '"192.168.0.0/16"'; do
  if grep -qF -- "$blanket" "$cfg"; then fail "ConfigBuilder must not exclude the blanket range $blanket from the tunnel"; fi
done
grep -qF 'CurrentPrivateNetworks()' "$cfg" || fail "Tun exclusions must come from the attached networks"
grep -qF 'ReplaceBlanketPrivateRanges(' "$repo_root/src/ui/mainwindow.cpp" || fail "the macOS system proxy bypass must use the attached networks"
echo "local_networks: OK"
