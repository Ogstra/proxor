#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
f="$repo_root/src/ui/mainwindow_grpc.cpp"

grep -qF 'ProxorPlatform::ClassifyResolvConf(ReadFileText("/etc/resolv.conf"))' "$f" || { echo "resolv.sh: ClassifyResolvConf not wired" >&2; exit 1; }
grep -q 'DirectDnsNoticeIsWarning' "$f" || { echo "resolv.sh: DirectDnsNoticeIsWarning not wired" >&2; exit 1; }
if grep -q 'may not works with systemd-resolved' "$f"; then echo "resolv.sh: stale systemd-resolved warning is still present" >&2; exit 1; fi
awk '/^#ifdef Q_OS_LINUX/{b=1} /^#endif/{b=0} b&&/ClassifyResolvConf\(/{ok=1} END{exit !ok}' "$f" || { echo "resolv.sh: ClassifyResolvConf must sit inside #ifdef Q_OS_LINUX" >&2; exit 1; }
if grep -n 'Q_OS_' "$repo_root"/src/platform/ResolvConf.* >&2; then echo "resolv.sh: Q_OS_ in the pure module" >&2; exit 1; fi
echo "resolv: OK"
