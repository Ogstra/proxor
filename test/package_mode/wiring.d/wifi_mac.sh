#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "wifi_mac.sh: $1" >&2; exit 1; }

mac="$repo_root/cmake/macos/macos.cmake"
for needle in 'WifiBackendMac.mm' 'CoreWLAN' '-fobjc-arc'; do
  grep -qF -- "$needle" "$mac" || fail "macos.cmake must mention $needle"
done
if grep -qF 'WifiBackendNone.cpp' "$mac"; then fail "macos.cmake must not use the None backend"; fi
for os in windows linux; do
  if grep -qE 'CoreWLAN|\.mm' "$repo_root/cmake/$os/$os.cmake"; then fail "$os.cmake must not mention CoreWLAN or .mm"; fi
done

backend="$repo_root/src/sys/wifi/WifiBackendMac.mm"
grep -qF 'ClassifyMacWifi(' "$backend" || fail "WifiBackendMac.mm must classify through ClassifyMacWifi"
if grep -qE 'NSTask|popen|QProcess' "$backend"; then fail "WifiBackendMac.mm must not spawn processes"; fi
if grep -n 'Q_OS_' "$repo_root"/src/sys/wifi/* "$repo_root"/src/platform/WifiMacClassify.* >&2; then
  fail "no Q_OS_ conditionals in the Wi-Fi sources"
fi
echo "wifi_mac: OK"
