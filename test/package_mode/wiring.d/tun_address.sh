#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"

cb="$repo_root/src/db/ConfigBuilder.cpp"
ds="$repo_root/src/main/ProxorGui_DataStore.hpp"

if grep -qF 'BuildTunAddressArray(dataStore->vpn_ipv6)' "$cb"; then
    echo "tun_address.sh: ConfigBuilder passes vpn_ipv6 straight to BuildTunAddressArray" >&2; exit 1
fi
grep -qF 'EffectiveTunIpv6(' "$cb" || { echo "tun_address.sh: EffectiveTunIpv6 not wired" >&2; exit 1; }
grep -qF 'ProxorPlatform::TunAddresses(' "$cb" || { echo "tun_address.sh: TunAddresses not used" >&2; exit 1; }

linux_branch="$(awk '/^#elif defined\(Q_OS_LINUX\)/{b=1;next} /^#else/{b=0} b' "$ds" | awk '1')"
echo "$linux_branch" | grep -qF 'bool vpn_ipv6 = true;' || { echo "tun_address.sh: Linux default for vpn_ipv6 is not true" >&2; exit 1; }
else_branch="$(awk '/^#elif defined\(Q_OS_LINUX\)/{s=1} s&&/^#else/{b=1;next} b&&/^#endif/{exit} b' "$ds")"
echo "$else_branch" | grep -qF 'bool vpn_ipv6 = false;' || { echo "tun_address.sh: fallback vpn_ipv6 default is not false" >&2; exit 1; }

if grep -n 'Q_OS_' "$repo_root"/src/platform/TunAddress.hpp "$repo_root"/src/platform/TunAddress.cpp; then
    echo "tun_address.sh: platform macros in the pure module" >&2; exit 1
fi
echo "tun_address: OK"
