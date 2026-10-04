#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "mac_autostart.sh: $1" >&2; exit 1; }
option=B # the letter on the MAC-AUTORUN: line of 53-RESEARCH.md ## Decisions

mac="$repo_root/cmake/macos/macos.cmake"
ar="$repo_root/src/sys/AutoRun.cpp"
for os in windows linux; do
  if grep -qE 'ServiceManagement|MacLoginItem' "$repo_root/cmake/$os/$os.cmake"; then fail "$os.cmake must not mention ServiceManagement or MacLoginItem"; fi
done
if grep -n 'Q_OS_' "$repo_root"/src/sys/macos/MacLoginItem.* "$repo_root"/src/platform/MacLoginItemPolicy.* >&2; then
  fail "no Q_OS_ conditionals in the login item sources"
fi
if grep -qF 'LSSharedFileList' "$ar"; then fail "AutoRun.cpp must not use LSSharedFileList"; fi
grep -qF 'DecideMacAutostartView(' "$ar" || fail "AutoRun.cpp must decide the checkbox through DecideMacAutostartView"
n="$(grep -c '^QString AutoRun_RefreshStaleEntry()' "$ar")"
[ "$n" = 3 ] || fail "AutoRun_RefreshStaleEntry defined $n times, expected 3"

case "$option" in
  B)
    for needle in 'MacLoginItem.mm' 'ServiceManagement'; do
      grep -qF -- "$needle" "$mac" || fail "macos.cmake must mention $needle"
    done
    grep -qF 'MacLaunchAgentPlist(' "$ar" || fail "AutoRun.cpp must write the agent through MacLaunchAgentPlist"
    grep -qF 'statusForLegacyURL' "$repo_root/src/sys/macos/MacLoginItem.mm" || fail "MacLoginItem.mm must read statusForLegacyURL"
    grep -qF '<string>-tray</string>' "$repo_root/test/package_mode/fixtures/macos-launch-agent.plist" || fail "the fixture must start Proxor with -tray"
    ;;
  *) fail "unsupported option $option" ;;
esac
echo "mac_autostart: OK"
