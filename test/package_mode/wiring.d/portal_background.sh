#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "portal_background.sh: $1" >&2; exit 1; }
pb="$repo_root/src/sys/linux/PortalBackground.cpp"
ar="$repo_root/src/sys/AutoRun.cpp"

grep -qF 'RequestBackground' "$pb" || fail "PortalBackground.cpp must call RequestBackground"
if grep -qF '// Stub from 52-04' "$pb"; then fail "PortalBackground.cpp is still the stub"; fi
grep -qF 'ProxorDesktop::RequestAutostart(' "$ar" || fail "AutoRun.cpp must use ProxorDesktop::RequestAutostart"
grep -qF 'FlatpakAutostartCommandline(' "$ar" || fail "AutoRun.cpp must build the Flatpak command line"
grep -qF 'flatpak-autostart' "$ar" || fail "AutoRun.cpp must keep the granted-state marker"
grep -qF 'ShouldRequestFlatpakAutostart(' "$ar" || fail "AutoRun.cpp must not re-request an unchanged state"

base="$(cd "$repo_root" && sh .planning/phases/52-linux-desktop-integration/phase-base.sh 2>/dev/null || true)"
if [ -n "$base" ] && command -v unifdef >/dev/null; then
  rel=src/sys/AutoRun.cpp
  for def in "-DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX" "-DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX"; do
    # shellcheck disable=SC2086
    # Phase 53 adds macOS-only hunks on purpose: judge the macOS view as of the phase-53 base.
    head_ref="HEAD"
    case "$def" in -DQ_OS_MACOS*) head_ref="$(cd "$repo_root" && sh .planning/phases/53-macos-desktop-integration/phase-base.sh 2>/dev/null || echo HEAD)";; esac
    if [ "$head_ref" = HEAD ]; then
      d=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 $def) <(unifdef -x2 $def "$repo_root/$rel") | grep -E '^[<>]' || true)
    else
      d=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 $def) <(git -C "$repo_root" show "$head_ref:$rel" | unifdef -x2 $def) | grep -E '^[<>]' || true)
    fi
    if [ -n "$d" ] && [ "$(printf '%s\n' "$d" | grep -vc 'AutoRun_RefreshStaleEntry')" != 0 ]; then
      fail "a non-Linux view of $rel changed beyond the 52-02 no-op: $d"
    fi
  done
fi
echo "portal_background: OK"
