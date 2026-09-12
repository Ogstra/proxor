#!/usr/bin/env bash
set -euo pipefail

# Builds the Arch package straight from a published GitHub Release, without the AUR. The
# recipe is the same template the AUR publication renders, so this path and `makepkg` from
# the AUR tree produce the same package.
usage() {
  printf 'Usage: %s <tag> [--output DIR] [--render-only]\n' "$0" >&2
  exit 2
}

tag="${1:-}"; shift || usage
case "$tag" in v[0-9]*.[0-9]*.[0-9]*) ;; *) usage;; esac
output= render_only=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --output) output="${2:-}"; shift 2 ;;
    --render-only) render_only=true; shift ;;
    *) usage ;;
  esac
done

root="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
version="${tag#v}"
output="${output:-$PWD/proxor-$version-arch}"
base="https://github.com/Ogstra/proxor/releases/download/$tag"
archive="proxor-$version.tar.gz"

mkdir -p "$output"
# SHA256SUMS is the release's own checksum list; the recipe pins the archive to the entry
# for it, so makepkg refuses a source that does not match what was published.
curl -fsSL -o "$output/SHA256SUMS" "$base/SHA256SUMS"
sha="$(awk -v name="./$archive" '$2 == name || $2 == "'"$archive"'" { print $1 }' "$output/SHA256SUMS")"
case "$sha" in
  [0-9a-f]*) ;;
  *) printf 'SHA256SUMS carries no entry for %s\n' "$archive" >&2; exit 1 ;;
esac

"$root/render-pkgbuild.sh" --version "$version" --url "$base/$archive" --sha256 "$sha" --output "$output" --pkgbuild-only
printf 'recipe for %s in %s\n' "$tag" "$output"

if "$render_only"; then
  exit 0
fi
command -v makepkg >/dev/null || { echo 'makepkg is required to build the package' >&2; exit 1; }
(cd "$output" && makepkg --syncdeps --install --cleanbuild)
