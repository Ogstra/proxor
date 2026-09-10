#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf '%s\n' "Usage: $0 --repo <clean-repository> --commit <sha> --version <X.Y.Z> --output <directory>" >&2
    exit 2
}

repo=""
commit=""
version=""
output=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --repo) repo="${2:-}"; shift 2 ;;
        --commit) commit="${2:-}"; shift 2 ;;
        --version) version="${2:-}"; shift 2 ;;
        --output) output="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done

[ -n "$repo" ] && [ -n "$commit" ] && [ -n "$version" ] && [ -n "$output" ] || usage
[ -d "$repo/.git" ] || { printf 'not a git worktree: %s\n' "$repo" >&2; exit 1; }
case "$version" in
    [0-9]*.[0-9]*.[0-9]*) ;;
    *) printf 'version must be X.Y.Z: %s\n' "$version" >&2; exit 1 ;;
esac

requested_commit="$(git -C "$repo" rev-parse --verify "${commit}^{commit}")"
head_commit="$(git -C "$repo" rev-parse HEAD)"
[ "$requested_commit" = "$head_commit" ] || {
    printf 'repository HEAD does not match requested commit\n' >&2
    exit 1
}
[ -z "$(git -C "$repo" status --porcelain --untracked-files=all)" ] || {
    printf 'source repository must be clean\n' >&2
    exit 1
}

submodules=(3rdparty/sing-box 3rdparty/QHotkey 3rdparty/SQLiteCpp)
for submodule in "${submodules[@]}"; do
    [ -d "$repo/$submodule/.git" ] || [ -f "$repo/$submodule/.git" ] || {
        printf 'submodule is not initialized: %s\n' "$submodule" >&2
        exit 1
    }
    expected="$(git -C "$repo" ls-tree "$requested_commit" "$submodule" | awk '{print $3}')"
    actual="$(git -C "$repo/$submodule" rev-parse HEAD)"
    [ "$expected" = "$actual" ] || {
        printf 'submodule does not match recorded commit: %s\n' "$submodule" >&2
        exit 1
    }
done
if git -C "$repo" submodule status --recursive | grep -Eq '^[-+]'; then
    printf '%s\n' 'recursive submodules are not initialized or do not match their recorded commits' >&2
    exit 1
fi

parent="$(dirname "$output")"
mkdir -p "$parent"
stage="$(mktemp -d "${parent}/.$(basename "$output").tmp.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
root="$stage/proxor-$version"
mkdir -p "$root"
git -C "$repo" archive "$requested_commit" | tar -xf - -C "$root"

while IFS=' ' read -r submodule_path submodule_commit; do
    [ -n "$submodule_path" ] || continue
    mkdir -p "$root/$submodule_path"
    git -C "$repo/$submodule_path" archive "$submodule_commit" | tar -xf - -C "$root/$submodule_path"
done < <(git -C "$repo" submodule foreach --quiet --recursive 'printf "%s %s\n" "$displaypath" "$sha1"')

archive="$stage/proxor-$version.tar.gz"
tar -C "$stage" -czf "$archive" "proxor-$version"
archive_sha="$(shasum -a 256 "$archive" | awk '{print $1}')"
{
    printf 'commit=%s\n' "$requested_commit"
    printf 'version=%s\n' "$version"
    for submodule in "${submodules[@]}"; do
        printf 'submodule.%s=%s\n' "${submodule//\//_}" "$(git -C "$repo/$submodule" rev-parse HEAD)"
    done
    printf 'sha256=%s\n' "$archive_sha"
} > "$stage/proxor-$version.source-manifest"

rm -rf "$output"
mv "$stage" "$output"
trap - EXIT
