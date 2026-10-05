#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
hpp="$repo_root/src/sub/GroupUpdater.hpp"
cpp="$repo_root/src/sub/GroupUpdater.cpp"
fail() { echo "wake_subs.sh: $*" >&2; exit 1; }

grep -q 'bool UI_has_due_subscription_updates();' "$hpp" || fail "declaration missing in GroupUpdater.hpp"
awk '/^bool UI_has_due_subscription_updates\(\) \{/{b=1} b&&/ShouldAutoUpdateGroupOnTimer/{ok=1} b&&/^}/{exit} END{exit !ok}' "$cpp" \
    || fail "UI_has_due_subscription_updates must use ShouldAutoUpdateGroupOnTimer"

if grep -n 'Q_OS_' "$repo_root"/src/platform/WakeSubscriptionRetry.*; then fail "Q_OS_ in WakeSubscriptionRetry"; fi
if grep -nE 'QTimer|ProxorGui::|dataStore' "$repo_root"/src/platform/WakeSubscriptionRetry.*; then fail "WakeSubscriptionRetry must stay pure"; fi
grep -qF '{3000, 10000, 30000, 60000}' "$repo_root/src/platform/WakeSubscriptionRetry.hpp" || fail "backoff delays not pinned in the header"
echo "wake_subs: OK"
