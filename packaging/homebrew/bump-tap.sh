#!/usr/bin/env bash
# Bump the Homebrew cask in Ogstra/homebrew-tap to a published Proxor release.
# The sha256 written to the cask is the hash of the downloaded asset, and only after it
# matches the release's SHA256SUMS. Idempotent, never downgrades, never pushes in --dry-run.
set -euo pipefail

usage() {
  echo "Usage: $0 --tag vX.Y.Z [--assets DIR] [--tap-url URL] [--work DIR] [--dry-run]" >&2
  exit 2
}

tag=""; assets_dir=""; tap_url="https://github.com/Ogstra/homebrew-tap.git"; work=""; dry_run=false
while [ "$#" -gt 0 ]; do
  case "$1" in
    --tag) [ "$#" -ge 2 ] || usage; tag="$2"; shift 2 ;;
    --assets) [ "$#" -ge 2 ] || usage; assets_dir="$2"; shift 2 ;;
    --tap-url) [ "$#" -ge 2 ] || usage; tap_url="$2"; shift 2 ;;
    --work) [ "$#" -ge 2 ] || usage; work="$2"; shift 2 ;;
    --dry-run) dry_run=true; shift ;;
    *) usage ;;
  esac
done
[ -n "$tag" ] || usage

printf '%s' "$tag" | grep -Eqx 'v[0-9]+\.[0-9]+\.[0-9]+' || { echo "invalid tag (expected vX.Y.Z): $tag" >&2; exit 1; }
version="${tag#v}"
zip="proxor-$version-macos-arm64.zip"
[ -n "$work" ] || work="${RUNNER_TEMP:-$(mktemp -d)}/homebrew"
here="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"

# Fail fast, before any download or clone.
if [ "$dry_run" = false ]; then
  case "$tap_url" in
    https://github.com/*)
      if [ -z "${HOMEBREW_TAP_TOKEN:-}" ]; then
        echo "HOMEBREW_TAP_TOKEN is required to push to $tap_url" >&2
        exit 1
      fi
      ;;
  esac
fi

mkdir -p "$work/assets"
if [ -n "$assets_dir" ]; then
  cp "$assets_dir/$zip" "$assets_dir/SHA256SUMS" "$work/assets/"
else
  gh release download "$tag" --repo "${GITHUB_REPOSITORY:-Ogstra/proxor}" \
    --pattern "$zip" --pattern SHA256SUMS --dir "$work/assets" --clobber
fi

expected="$(awk -v f="$zip" '$2 == f || $2 == "./" f || $2 == "*" f {print $1}' "$work/assets/SHA256SUMS")"
if [ -z "$expected" ] || [ "$(printf '%s\n' "$expected" | wc -l | tr -d ' ')" != 1 ]; then
  echo "SHA256SUMS has no single entry for $zip" >&2
  exit 1
fi
actual="$(shasum -a 256 "$work/assets/$zip" | awk '{print $1}')"
if [ "$expected" != "$actual" ]; then
  echo "checksum mismatch for $zip: SHA256SUMS says $expected, the asset is $actual" >&2
  exit 1
fi

# The Intel zip is optional: only a release whose SHA256SUMS lists it gets a two-architecture cask.
intel_zip="proxor-$version-macos-x86_64.zip"
intel_expected="$(awk -v f="$intel_zip" '$2 == f || $2 == "./" f || $2 == "*" f {print $1}' "$work/assets/SHA256SUMS")"
intel_args=()
if [ -n "$intel_expected" ]; then
  if [ "$(printf '%s\n' "$intel_expected" | wc -l | tr -d ' ')" != 1 ]; then
    echo "SHA256SUMS has more than one entry for $intel_zip" >&2
    exit 1
  fi
  if [ -n "$assets_dir" ]; then
    cp "$assets_dir/$intel_zip" "$work/assets/"
  else
    gh release download "$tag" --repo "${GITHUB_REPOSITORY:-Ogstra/proxor}" \
      --pattern "$intel_zip" --dir "$work/assets" --clobber
  fi
  intel_actual="$(shasum -a 256 "$work/assets/$intel_zip" | awk '{print $1}')"
  if [ "$intel_expected" != "$intel_actual" ]; then
    echo "checksum mismatch for $intel_zip: SHA256SUMS says $intel_expected, the asset is $intel_actual" >&2
    exit 1
  fi
  intel_args=(--sha256-intel "$intel_actual")
fi

# The tap is public: an anonymous clone works and keeps the token off the command line.
rm -rf "$work/tap"
git clone -q "$tap_url" "$work/tap"

if [ -f "$work/tap/Casks/proxor.rb" ]; then
  current="$(sed -n 's/^  version "\(.*\)"$/\1/p' "$work/tap/Casks/proxor.rb")"
  if [ -n "$current" ] && [ "$current" != "$version" ] \
    && [ "$(printf '%s\n%s\n' "$current" "$version" | sort -V | tail -1)" = "$current" ]; then
    echo "refusing to downgrade the tap from $current to $version" >&2
    exit 1
  fi
fi

# bash 3.2 + set -u: an empty array must not be expanded bare.
"$here/render-cask.sh" --version "$version" --sha256 "$actual" ${intel_args[@]+"${intel_args[@]}"} --output "$work/tap/Casks"

cd "$work/tap"
if [ -z "$(git status --porcelain)" ]; then
  echo "the tap already carries proxor $version"
  exit 0
fi

git config user.name 'Proxor release automation'
git config user.email 'maintainers@ogstra.github.io'
git add Casks/proxor.rb
git -c commit.gpgsign=false commit -q -m "proxor $version"

if [ "$dry_run" = true ]; then
  git --no-pager show --stat HEAD
  echo "dry run: not pushing proxor $version to $tap_url"
  exit 0
fi

case "$tap_url" in
  https://github.com/*)
    # The token reaches git only through gh's credential helper: never a URL, never argv,
    # and nothing is written to the global git config.
    GH_TOKEN="$HOMEBREW_TAP_TOKEN" git -c credential.helper= \
      -c 'credential.helper=!gh auth git-credential' push origin HEAD:main
    ;;
  *)
    git push -q origin HEAD:main
    ;;
esac
echo "pushed proxor $version to $tap_url"
