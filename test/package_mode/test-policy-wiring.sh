#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
for anchor in CheckUpdate StartVPNProcess SetSystemProxy FindCoreAsset; do
    if ! grep -Rqs "$anchor" "$repo_root/src"; then
        printf 'missing production anchor: %s\n' "$anchor" >&2
        exit 1
    fi
done

printf '%s\n' 'package policy is not wired into production entry points yet' >&2
exit 1
