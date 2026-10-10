#!/usr/bin/env bash
set -euo pipefail
script="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)/render-cask.sh"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT
sha="$(printf x | shasum -a 256 | awk '{print $1}')"

"$script" --version 1.2.3 --sha256 "$sha" --output "$tmp/ok"
cask="$tmp/ok/proxor.rb"
while IFS= read -r line; do
  grep -qxF -- "$line" "$cask" || { echo "FAIL: missing line: $line" >&2; exit 1; }
done <<LINES
cask "proxor" do
  version "1.2.3"
  sha256 "$sha"
  url "https://github.com/Ogstra/proxor/releases/download/v#{version}/proxor-#{version}-macos-arm64.zip"
  name "Proxor"
  homepage "https://github.com/Ogstra/proxor"
  auto_updates false
  depends_on arch: :arm64
  depends_on macos: :sequoia
  app "Proxor.app"
  postflight_steps do
    run "/usr/bin/xattr", args: ["-dr", "com.apple.quarantine", "{{appdir}}/Proxor.app"]
  uninstall quit: "io.github.Ogstra.Proxor"
  zap launchctl: "io.github.Ogstra.Proxor.helper",
        "/Library/Application Support/Proxor",
        "/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist",
        "/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper",
        "/var/log/proxor-helper.log",
        "~/Library/LaunchAgents/io.github.Ogstra.Proxor.autostart.plist",
        "~/Library/Preferences/io.github.Ogstra.Proxor.plist",
        "~/Library/Preferences/proxor",
LINES

if grep -Eq '^[[:space:]]*postflight do|verified:|@[A-Z0-9_]+@' "$cask"; then
  echo 'FAIL: deprecated DSL or unreplaced placeholder in rendered cask' >&2; exit 1
fi

"$script" --version 1.2.3 --sha256 "$sha" --output "$tmp/ok2"
cmp "$cask" "$tmp/ok2/proxor.rb"

ruby -c "$cask" >/dev/null

# The Proxor service must survive `brew upgrade` (Homebrew runs the uninstall block on upgrade): it is removed by zap only.
if [ "$(grep -c 'launchctl:' "$cask")" != 1 ]; then
  echo 'FAIL: expected exactly one launchctl: directive' >&2; exit 1
fi
uninstall_block="$(awk '/^  uninstall/{f=1} /^  zap/{f=0} f' "$cask")"
zap_block="$(awk '/^  zap/{f=1} f' "$cask")"
if grep -qE 'launchctl|delete|PrivilegedHelperTools|LaunchDaemons|Application Support' <<<"$uninstall_block"; then
  echo 'FAIL: the uninstall block must not remove the Proxor service' >&2; exit 1
fi
for path in \
  "/Library/Application Support/Proxor" \
  "/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist" \
  "/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper" \
  "~/Library/LaunchAgents/io.github.Ogstra.Proxor.autostart.plist"; do
  grep -qF -- "\"$path\"" <<<"$zap_block" || { echo "FAIL: zap block lacks path: $path" >&2; exit 1; }
done
if ! grep -q 'launchctl:' <<<"$zap_block"; then
  echo 'FAIL: launchctl: missing from the zap block' >&2; exit 1
fi

n=0
expect_reject() {
  n=$((n + 1)); d="$tmp/bad-$n"
  if "$script" "$@" --output "$d" >/dev/null 2>&1; then echo "FAIL: case $n accepted: $*" >&2; exit 1; fi
  if [ -e "$d/proxor.rb" ]; then echo "FAIL: case $n wrote proxor.rb: $*" >&2; exit 1; fi
}
expect_reject --version 1.2 --sha256 "$sha"
expect_reject --version v1.2.3 --sha256 "$sha"
expect_reject --version 1.2.3-beta --sha256 "$sha"
expect_reject --version 1.2.3 --sha256 "${sha%?}"
expect_reject --version 1.2.3 --sha256 "$(printf %s "$sha" | tr a-f A-F)"
# wrong order and missing --output
n=$((n + 1))
if "$script" --sha256 "$sha" --version 1.2.3 --output "$tmp/bad-$n" >/dev/null 2>&1; then echo 'FAIL: wrong arg order accepted' >&2; exit 1; fi
test ! -e "$tmp/bad-$n/proxor.rb"
if (cd "$tmp" && "$script" --version 1.2.3 --sha256 "$sha" >/dev/null 2>&1); then echo 'FAIL: missing --output accepted' >&2; exit 1; fi

# --- two-architecture cask (--sha256-intel) ---
sha2="$(printf z | shasum -a 256 | awk '{print $1}')"
"$script" --version 1.2.3 --sha256 "$sha" --sha256-intel "$sha2" --output "$tmp/two"
two="$tmp/two/proxor.rb"
while IFS= read -r line; do
  grep -qxF -- "$line" "$two" || { echo "FAIL: two-arch cask missing line: $line" >&2; exit 1; }
done <<LINES
cask "proxor" do
  arch arm: "arm64", intel: "x86_64"
  version "1.2.3"
  sha256 arm:   "$sha",
         intel: "$sha2"
  on_arm do
    depends_on macos: :sequoia
  on_intel do
    depends_on macos: :monterey
  url "https://github.com/Ogstra/proxor/releases/download/v#{version}/proxor-#{version}-macos-#{arch}.zip"
LINES
if grep -Eq 'depends_on arch:|^  depends_on macos:|@[A-Z0-9_]+@' "$two"; then
  echo 'FAIL: two-arch cask has depends_on arch:, a top-level depends_on macos: or a placeholder' >&2; exit 1
fi
ruby -c "$two" >/dev/null
"$script" --version 1.2.3 --sha256 "$sha" --sha256-intel "$sha2" --output "$tmp/two2"
cmp "$two" "$tmp/two2/proxor.rb"

# Drift guard: from `name "Proxor"` on, the two templates agree except for depends_on lines.
tail_of() { sed -n '/^  name "Proxor"/,$p' "$1" | grep -v -E '^  depends_on '; }
tail_of "$(dirname "$script")/proxor.rb.in" > "$tmp/tail-arm"
tail_of "$(dirname "$script")/proxor-two-arch.rb.in" > "$tmp/tail-two"
[ -s "$tmp/tail-arm" ] || { echo 'FAIL: empty template tail' >&2; exit 1; }
cmp "$tmp/tail-arm" "$tmp/tail-two" || { echo 'FAIL: the two cask templates drifted apart' >&2; exit 1; }

expect_reject --version 1.2.3 --sha256 "$sha" --sha256-intel "${sha2%?}"
expect_reject --version 1.2.3 --sha256 "$sha" --sha256-intel "$(printf %s "$sha2" | tr a-f A-F)"
expect_reject --version 1.2.3 --sha256 "$sha" --sha256-intel "$sha"
n=$((n + 1))
if "$script" --version 1.2.3 --sha256-intel "$sha2" --sha256 "$sha" --output "$tmp/bad-$n" >/dev/null 2>&1; then echo 'FAIL: wrong two-arch arg order accepted' >&2; exit 1; fi
test ! -e "$tmp/bad-$n/proxor.rb"
if (cd "$tmp" && "$script" --version 1.2.3 --sha256 "$sha" --sha256-intel "$sha2" >/dev/null 2>&1); then echo 'FAIL: two-arch render without --output accepted' >&2; exit 1; fi

# brew style (read-only, a copy under $tmp): only the three offenses that come from sitting outside a tap are allowed.
if command -v brew >/dev/null 2>&1; then
  mkdir -p "$tmp/style"; cp "$two" "$tmp/style/proxor.rb"
  style_out="$(HOMEBREW_NO_AUTO_UPDATE=1 HOMEBREW_NO_INSTALL_FROM_API=1 brew style "$tmp/style/proxor.rb" 2>&1 || true)"
  extra="$(printf '%s\n' "$style_out" | grep -E '^[^ ]+:[0-9]+:[0-9]+: [CWEF]: ' | grep -v -E 'Sorbet/(StrictSigil|TrueSigil)|Style/FrozenStringLiteralComment' || true)"
  [ -z "$extra" ] || { echo "FAIL: brew style offenses in the two-arch cask:" >&2; printf '%s\n' "$extra" >&2; exit 1; }
else
  echo "test-render-cask.sh: brew not found, skipping brew style"
fi

echo "test-render-cask.sh: OK"
