#!/usr/bin/env bash
set -euo pipefail
trap 'printf "source staging test failed at line %s\n" "$LINENO" >&2' ERR

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
helper="$repo_root/packaging/source/stage-recursive-source.sh"
provenance="$repo_root/packaging/toolchains/PROVENANCE.md"

test -x "$helper" || { printf '%s\n' 'source staging helper is missing' >&2; exit 1; }
test -f "$provenance" || { printf '%s\n' 'toolchain provenance is missing' >&2; exit 1; }
! grep -Eqi '(TBD|TODO|placeholder|<[^>]+>)' "$provenance" || {
    printf '%s\n' 'toolchain provenance contains unresolved values' >&2
    exit 1
}
for image in ubuntu-22.04 debian-12 fedora arch; do
    grep -Eq "${image}.*@sha256:[0-9a-f]{64}" "$provenance" || {
        printf 'missing immutable image provenance for %s\n' "$image" >&2
        exit 1
    }
done
grep -Eq 'actions/checkout@[^[:space:]]+.*[0-9a-f]{40}' "$provenance"
grep -Eq 'actions/setup-go@[^[:space:]]+.*[0-9a-f]{40}' "$provenance"
for action in actions/upload-artifact actions/download-artifact actions/cache jurplel/install-qt-action ilammy/msvc-dev-cmd seanmiddleditch/gha-setup-ninja; do
    grep -Eq "${action}@[^[:space:]]+.*[0-9a-f]{40}" "$provenance" || {
        printf 'missing immutable action provenance for %s\n' "$action" >&2
        exit 1
    }
done

work="$(mktemp -d "${TMPDIR:-/tmp}/proxor-source-stage.XXXXXX")"
trap 'rm -rf "$work"' EXIT
commit="$(git -C "$repo_root" rev-parse HEAD)"
version="$(tr -d '\r\n' < "$repo_root/VERSION.txt")"
clone="$work/clone"
git clone --quiet "$repo_root" "$clone"
git -C "$clone" checkout --quiet --detach "$commit"
git -C "$clone" submodule update --init \
    3rdparty/sing-box 3rdparty/QHotkey 3rdparty/SQLiteCpp

output="$work/output"
"$helper" --repo "$clone" --commit "$commit" --version "$version" --output "$output"

root="$output/proxor-$version"
archive="$output/proxor-$version.tar.gz"
test -d "$root"
test -s "$archive"
archive_list="$work/archive-list"
tar tzf "$archive" > "$archive_list"
for submodule in 3rdparty/sing-box 3rdparty/QHotkey 3rdparty/SQLiteCpp; do
    test -d "$root/$submodule"
    grep -q "^proxor-$version/$submodule/" "$archive_list"
done
! find "$root" -name .git | grep -q .
! grep -Eq '(^|/)\.git(/|$)' "$archive_list"
grep -Fx "commit=$commit" "$output/proxor-$version.source-manifest"
grep -Fx "version=$version" "$output/proxor-$version.source-manifest"

printf 'local-only\n' > "$clone/local-only"
if "$helper" --repo "$clone" --commit "$commit" --version "$version" --output "$work/dirty-output"; then
    printf '%s\n' 'expected dirty source input to be rejected' >&2
    exit 1
fi
