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
  uninstall launchctl: "io.github.Ogstra.Proxor.helper",
            quit:      "io.github.Ogstra.Proxor",
              "/Library/Application Support/Proxor",
              "/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist",
              "/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper",
  zap delete: "/var/log/proxor-helper.log",
        "~/Library/Preferences/io.github.Ogstra.Proxor.plist",
        "~/Library/Preferences/proxor",
LINES

if grep -Eq '^[[:space:]]*postflight do|verified:|@[A-Z0-9_]+@' "$cask"; then
  echo 'FAIL: deprecated DSL or unreplaced placeholder in rendered cask' >&2; exit 1
fi

"$script" --version 1.2.3 --sha256 "$sha" --output "$tmp/ok2"
cmp "$cask" "$tmp/ok2/proxor.rb"

ruby -c "$cask" >/dev/null

# Helper removal stanzas: launchctl exactly once and before quit; delete paths inside the uninstall block.
if [ "$(grep -c 'launchctl:' "$cask")" != 1 ]; then
  echo 'FAIL: expected exactly one launchctl: directive' >&2; exit 1
fi
launchctl_line="$(grep -n 'launchctl:' "$cask" | cut -d: -f1)"
quit_line="$(grep -n 'quit:' "$cask" | cut -d: -f1)"
if [ "$launchctl_line" -ge "$quit_line" ]; then
  echo 'FAIL: launchctl: must appear before quit:' >&2; exit 1
fi
uninstall_block="$(awk '/^  uninstall/{f=1} /^  zap/{f=0} f' "$cask")"
for path in \
  "/Library/Application Support/Proxor" \
  "/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist" \
  "/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper"; do
  grep -qF -- "\"$path\"" <<<"$uninstall_block" || { echo "FAIL: uninstall block lacks delete path: $path" >&2; exit 1; }
done
if ! grep -q 'delete:' <<<"$uninstall_block"; then
  echo 'FAIL: delete: missing from the uninstall block' >&2; exit 1
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

echo "test-render-cask.sh: OK"
