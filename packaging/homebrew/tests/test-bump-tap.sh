#!/usr/bin/env bash
set -euo pipefail
root="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"; script="$root/bump-tap.sh"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
: > "$tmp/gitconfig"; export GIT_CONFIG_GLOBAL="$tmp/gitconfig" GIT_CONFIG_NOSYSTEM=1
unset HOMEBREW_TAP_TOKEN
git init -q -b main "$tmp/seed"; printf '# tap\n' > "$tmp/seed/README.md"
git -C "$tmp/seed" add README.md; git -C "$tmp/seed" -c user.name=t -c user.email=t@t commit -qm init
git clone -q --bare "$tmp/seed" "$tmp/tap.git"; tap="file://$tmp/tap.git"

# assets VERSION [bad]: writes $tmp/assets-VERSION/{proxor-VERSION-macos-arm64.zip,SHA256SUMS} in the ./name format
assets() {
  d="$tmp/assets-$1"; mkdir -p "$d"; printf 'zip %s' "$1" > "$d/proxor-$1-macos-arm64.zip"
  h="$(shasum -a 256 "$d/proxor-$1-macos-arm64.zip" | awk '{print $1}')"
  [ "${2:-}" != bad ] || h="$(printf y | shasum -a 256 | awk '{print $1}')"
  printf '%s  ./proxor-%s.tar.gz\n%s  ./proxor-%s-macos-arm64.zip\n' "$h" "$1" "$h" "$1" > "$d/SHA256SUMS"
  echo "$d"
}
head_of() { git --git-dir "$tmp/tap.git" rev-parse main; }
cask_version() { git --git-dir "$tmp/tap.git" show main:Casks/proxor.rb | sed -n 's/^  version "\(.*\)"$/\1/p'; }
fail() { echo "FAIL: $*" >&2; exit 1; }

# 1. first bump
"$script" --tag v1.2.3 --assets "$(assets 1.2.3)" --tap-url "$tap" --work "$tmp/w1" >/dev/null
[ "$(cask_version)" = 1.2.3 ] || fail "case 1 version"
[ "$(git --git-dir "$tmp/tap.git" log -1 --format=%s main)" = "proxor 1.2.3" ] || fail "case 1 subject"
zsha="$(shasum -a 256 "$tmp/assets-1.2.3/proxor-1.2.3-macos-arm64.zip" | awk '{print $1}')"
git --git-dir "$tmp/tap.git" show main:Casks/proxor.rb | grep -qxF "  sha256 \"$zsha\"" || fail "case 1 sha256"
if git --git-dir "$tmp/tap.git" log -1 --format=%B main | grep -qi 'co-authored-by\|claude'; then fail "case 1 attribution trailer"; fi
before="$(head_of)"

# 2. repeat is a no-op
out="$("$script" --tag v1.2.3 --assets "$tmp/assets-1.2.3" --tap-url "$tap" --work "$tmp/w2" 2>&1)" || fail "case 2 exit"
grep -qF 'the tap already carries proxor 1.2.3' <<<"$out" || fail "case 2 message"
[ "$(head_of)" = "$before" ] || fail "case 2 moved main"

# 3. downgrade refused
if out="$("$script" --tag v1.2.2 --assets "$(assets 1.2.2)" --tap-url "$tap" --work "$tmp/w3" 2>&1)"; then fail "case 3 accepted"; fi
grep -qF 'refusing to downgrade' <<<"$out" || fail "case 3 message"
[ "$(head_of)" = "$before" ] || fail "case 3 moved main"

# 4. checksum mismatch refused
if out="$("$script" --tag v1.2.4 --assets "$(assets 1.2.4 bad)" --tap-url "$tap" --work "$tmp/w4" 2>&1)"; then fail "case 4 accepted"; fi
grep -qF 'checksum mismatch' <<<"$out" || fail "case 4 message"
[ "$(head_of)" = "$before" ] || fail "case 4 moved main"

# 5. missing token for the default https tap URL fails before cloning
if out="$(env -u HOMEBREW_TAP_TOKEN "$script" --tag v1.2.4 --assets "$(assets 1.2.4)" --work "$tmp/w5" 2>&1)"; then fail "case 5 accepted"; fi
grep -qF 'HOMEBREW_TAP_TOKEN is required' <<<"$out" || fail "case 5 message"
[ ! -e "$tmp/w5/tap" ] || fail "case 5 cloned before checking the token"

# 6. dry run leaves main alone, a real run moves it
"$script" --tag v1.2.4 --assets "$(assets 1.2.4)" --tap-url "$tap" --work "$tmp/w6" --dry-run >/dev/null
[ "$(head_of)" = "$before" ] || fail "case 6 dry run moved main"
"$script" --tag v1.2.4 --assets "$(assets 1.2.4)" --tap-url "$tap" --work "$tmp/w6b" >/dev/null
[ "$(cask_version)" = 1.2.4 ] || fail "case 6 real run"

# 7. tag without leading v is rejected
if "$script" --tag 1.2.3 --assets "$(assets 1.2.3)" --tap-url "$tap" --work "$tmp/w7" >/dev/null 2>&1; then fail "case 7 accepted"; fi

echo "test-bump-tap.sh: OK"
