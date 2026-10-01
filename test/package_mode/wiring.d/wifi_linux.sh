#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "wifi_linux.sh: $1" >&2; exit 1; }

lin="$repo_root/cmake/linux/linux.cmake"
grep -qF 'WifiBackendLinux.cpp' "$lin" || fail "linux.cmake must list WifiBackendLinux.cpp"
grep -qF 'WifiBackendNetworkManager.cpp' "$lin" || fail "linux.cmake must list WifiBackendNetworkManager.cpp"
grep -qF '::DBus' "$lin" || fail "linux.cmake must link QtDBus"
if grep -qF 'WifiBackendNone.cpp' "$lin"; then fail "linux.cmake must not use the None backend"; fi

for os in windows macos; do
  if grep -qiE 'NetworkManager|DBus' "$repo_root/cmake/$os/$os.cmake"; then fail "$os.cmake must not mention NetworkManager or DBus"; fi
done

grep -qF '/.flatpak-info' "$repo_root/src/sys/wifi/WifiBackendLinux.cpp" || fail "Linux backend lost the Flatpak detection"
grep -qF 'systemBus()' "$repo_root/src/sys/wifi/WifiBackendLinux.cpp" || fail "Linux backend must use the system bus"
nm="$repo_root/src/sys/wifi/WifiBackendNetworkManager.cpp"
for needle in 'LC_ALL' '--rescan' 'org.freedesktop.NetworkManager.Device.Wireless'; do
  grep -qF -- "$needle" "$nm" || fail "WifiBackendNetworkManager.cpp lost $needle"
done
grep -qxF '  - --system-talk-name=org.freedesktop.NetworkManager' "$repo_root/packaging/flatpak/io.github.Ogstra.Proxor.yml" \
  || fail "Flatpak manifest lost the NetworkManager talk-name"
if grep -rn 'Q_OS_' "$repo_root/src/sys/wifi" >&2; then fail "no Q_OS_ in the Wi-Fi sources"; fi
echo "wifi_linux: OK"
