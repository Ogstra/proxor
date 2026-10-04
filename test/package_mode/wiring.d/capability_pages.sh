#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
ui="$repo_root/src/ui"

need() { grep -q "$2" "$ui/$1" || { echo "capability_pages.sh: $1 must consult $2" >&2; exit 1; }; }
need dialog_basic_settings.cpp 'Capability::AutoStart'
need dialog_basic_settings.cpp 'Capability::IcmpPing'
need dialog_ssid_settings.cpp 'Capability::OnDemandSsid'
need dialog_manage_routes.cpp 'Capability::OnDemandSsid'
need dialog_vpn_settings.cpp 'Capability::TunMode'
need dialog_vpn_settings.cpp 'Capability::TunStrictRoute'
need dialog_vpn_settings.cpp 'Capability::TunSingleCore'

grep -B1 'AutoRun_SetEnabled(' "$ui/dialog_basic_settings.cpp" | grep -q IsUsable \
  || { echo "capability_pages.sh: AutoRun_SetEnabled must be gated by IsUsable" >&2; exit 1; }

phase_base="$repo_root/.planning/phases/54-cross-platform-correctness-and-never-silent/phase-base.sh"
if [ -f "$phase_base" ]; then
  base="$(cd "$repo_root" && sh "$phase_base" 2>/dev/null || true)"
  if [ -n "$base" ]; then
    # Q_OS_MACOS blocks are the phase 53 macOS-only additions (Windows/Linux views are proven by unifdef in 53-02/53-05).
    added="$(cd "$repo_root" && git diff "$base" -- src/ui/dialog_basic_settings.cpp src/ui/dialog_ssid_settings.cpp src/ui/dialog_vpn_settings.cpp src/ui/dialog_manage_routes.cpp | grep '^+.*Q_OS_' | grep -vc 'Q_OS_MACOS' || true)"
    [ "$added" = "0" ] || { echo "capability_pages.sh: pages must not gain Q_OS_ conditionals" >&2; exit 1; }
  fi
fi
echo "capability_pages: OK"
