#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"

go_prefix="$(sed -n 's/^const LocalIcmpErrorPrefix = "\(.*\)"$/\1/p' "$repo_root/go/proxorlib/speedtest/icmp.go")"
[ -n "$go_prefix" ] || { echo "ping.sh: LocalIcmpErrorPrefix not found in icmp.go" >&2; exit 1; }
grep -qF "kLocalIcmpErrorPrefix = \"$go_prefix\"" "$repo_root/src/platform/PingPolicy.hpp" || { echo "ping.sh: Go and C++ ICMP prefixes differ" >&2; exit 1; }
f="$repo_root/src/ui/mainwindow_grpc.cpp"
grep -q 'ProxorPlatform::EffectivePingMode(mode, icmp_unavailable_this_session' "$f" || { echo "ping.sh: EffectivePingMode not wired" >&2; exit 1; }
grep -q 'ProxorPlatform::ClassifyPingResult(effectiveMode' "$f" || { echo "ping.sh: ClassifyPingResult not wired" >&2; exit 1; }
grep -q 'ProxorPlatform::IcmpFallbackNotice' "$f" || { echo "ping.sh: fallback notice not wired" >&2; exit 1; }
grep -q 'static_assert(libcore::TcpPing == ProxorPlatform::kTcpPingMode' "$f" || { echo "ping.sh: mode static_assert missing" >&2; exit 1; }
awk '/PingOutcome::RetryWithTcp/{b=1} b&&/DisplayAddress\(\)/{ok=1} END{exit !ok}' "$f" || { echo "ping.sh: the TCP fallback must use host:port (DisplayAddress)" >&2; exit 1; }
