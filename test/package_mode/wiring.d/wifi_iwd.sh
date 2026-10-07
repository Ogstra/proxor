#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "wifi_iwd.sh: $1" >&2; exit 1; }

need() { # file needle
  grep -qF -- "$2" "$1" || fail "$1 must contain $2"
}

lin="$repo_root/cmake/linux/linux.cmake"
need "$lin" 'src/sys/wifi/WifiBackendIwd.cpp'
need "$lin" 'src/sys/wifi/WifiBackendChain.cpp'
for os in windows macos; do
  if grep -qiE 'WifiBackendIwd|WifiBackendChain|connman' "$repo_root/cmake/$os/$os.cmake"; then
    fail "$os.cmake must not mention the iwd or chain readers"
  fi
done

fac="$repo_root/src/sys/wifi/WifiBackendLinux.cpp"
for needle in 'make_unique<NmThenIwdWifiReader>' 'make_unique<IwdWifiReader>' 'systemBus()' '/.flatpak-info'; do
  need "$fac" "$needle"
done

iwd="$repo_root/src/sys/wifi/WifiBackendIwd.cpp"
for needle in 'net.connman.iwd' 'GetManagedObjects' 'net.connman.iwd.Station' 'ConnectedNetwork' 'setAutoStartService(false)'; do
  need "$iwd" "$needle"
done
need "$repo_root/src/sys/wifi/WifiBackendChain.cpp" 'ChooseWifiReading'

if grep -qF 'connman' "$repo_root/packaging/flatpak/io.github.Ogstra.Proxor.yml"; then
  fail "Flatpak manifest must not gain an iwd/connman permission"
fi

docs="$repo_root/docs/Run_Linux.md"
grep -qxF '## Wayland compositors' "$docs" || fail "Run_Linux.md lost the Wayland compositors section"
grep -q '^## Wi-Fi network detection' "$docs" || fail "Run_Linux.md lost the Wi-Fi network detection section"
grep -qF 'iwd' "$repo_root/packaging/release/notes/v1.6.14.md" || fail "release notes v1.6.14 must mention iwd"

if grep -rn 'Q_OS_' "$repo_root/src/sys/wifi" >&2; then fail "no Q_OS_ in the Wi-Fi sources"; fi
echo "wifi_iwd: OK"
