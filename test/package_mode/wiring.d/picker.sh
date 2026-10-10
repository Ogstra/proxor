#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "picker.sh: $*" >&2; exit 1; }
f="$repo_root/src/ui/dialog_vpn_settings.cpp"

grep -q 'ListLinuxProcessNames()' "$f" || fail "picker must read /proc through ListLinuxProcessNames()"
grep -q 'CompiledHostOs() == ProxorPlatform::HostOs::Linux' "$f" || fail "picker must branch on the runtime Linux check"
grep -q 'readProc ? QString()' "$f" || fail "ps output must be ignored when /proc was read"
grep -q 'tasklist' "$f" || fail "Windows tasklist path must stay"
grep -q "section(QLatin1Char('/'), -1)" "$f" || fail "macOS basename line must stay"
if grep -nE 'QProcess|Q_OS_' "$repo_root/src/platform/ProcessNames.cpp"; then
  fail "ProcessNames.cpp must stay Qt Core only without QProcess or Q_OS_"
fi

base="$(cd "$repo_root" && sh .planning/phases/52-linux-desktop-integration/phase-base.sh 2>/dev/null || true)"
if [ -n "$base" ] && command -v unifdef >/dev/null; then
  rel=src/ui/dialog_vpn_settings.cpp
  w=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX) <(unifdef -x2 -DQ_OS_WIN -UQ_OS_MACOS -UQ_OS_LINUX "$repo_root/$rel") | grep -E '^[<>]' | sort || true)
  m=$(diff <(git -C "$repo_root" show "$base:$rel" | unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX) <(unifdef -x2 -DQ_OS_MACOS -UQ_OS_WIN -UQ_OS_LINUX "$repo_root/$rel") | grep -E '^[<>]' | sort || true)
  # The macOS-only service group was renamed from "Tun service" to "Proxor service" (one name for the helper, UX review A8).
  # Those exact renamed strings are the only macOS-only edits ignored here; any other divergence still fails.
  renamed='tr\("(Tun|Proxor) service"\)|tr\("Remove (Tun|Proxor) service"\)|Remove the Proxor (Tun )?service\?'
  w=$(printf '%s\n' "$w" | grep -vE "$renamed" || true)
  m=$(printf '%s\n' "$m" | grep -vE "$renamed" || true)
  [ "$w" = "$m" ] || fail "Windows and macOS views of $rel changed differently"
fi
echo "picker: OK"
