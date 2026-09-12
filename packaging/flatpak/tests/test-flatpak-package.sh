#!/usr/bin/env bash
set -euo pipefail

root="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)"
manifest="$root/packaging/flatpak/io.github.Ogstra.Proxor.yml"
wrapper="$root/packaging/flatpak/proxor-wrapper.sh"

test -f "$manifest"
test -x "$wrapper"
# The release tarball carries git's recorded modes, so a script that is only
# executable in the working tree still fails inside flatpak-builder.
for script in packaging/flatpak/build-offline.sh packaging/flatpak/proxor-wrapper.sh libs/build_go.sh; do
  test -x "$root/$script"
  test "$(git -C "$root" ls-files -s "$script" | cut -d' ' -f1)" = 100755
done
grep -qx 'exec /app/lib/proxor/proxor "$@"' "$wrapper"
grep -qx 'app-id: io.github.Ogstra.Proxor' "$manifest"
grep -qx 'runtime: org.kde.Platform' "$manifest"
grep -qx 'runtime-version: "6.8"' "$manifest"
grep -Fqx '  - --share=ipc' "$manifest"
grep -Fqx '  - --socket=wayland' "$manifest"
grep -Fqx '  - --socket=fallback-x11' "$manifest"
grep -Fqx '  - --device=dri' "$manifest"
grep -Fqx '  - --share=network' "$manifest"
test "$(grep -c '^  - --' "$manifest")" -eq 5
! grep -Eq -- '--device=all|--filesystem=host|--socket=system-bus|--talk-name=|pkexec|setcap|curl|go[[:space:]]+mod[[:space:]]+download' "$manifest" "$wrapper" "$root/packaging/flatpak/build-offline.sh"
grep -Fq 'generated-go-sources.json' "$manifest"
grep -Fqx '        dest: .flatpak-input' "$manifest"
grep -Fqx '        dest: .flatpak-input/geodata' "$manifest"
grep -Fqx '        dest: .flatpak-input/go-cache' "$manifest"
grep -Fq '"$prefix/share/proxor/$asset"' "$root/packaging/flatpak/build-offline.sh"
grep -Fq 'build-offline.sh "$PWD" /app' "$manifest"
grep -Fq 'FLATPAK_ID=io.github.Ogstra.Proxor' "$manifest"

# The runtime has no Go: the toolchain is pinned by checksum and the offline build
# resolves modules from the staged proxy only, never from a toolchain download.
go_version="$(sed -n 's/^ *GO_VERSION: *"\{0,1\}\([^"]*\)"\{0,1\} *$/\1/p' "$root/.github/workflows/build-proxor-cmake.yml" | head -n1)"
test -n "$go_version"
grep -Fqx "        url: https://go.dev/dl/go$go_version.linux-amd64.tar.gz" "$manifest"
grep -Eq '^        sha256: [0-9a-f]{64}$' "$manifest"
grep -Fqx '        dest: .flatpak-go-toolchain' "$manifest"
grep -Fq 'export GOTOOLCHAIN=local' "$root/packaging/flatpak/build-offline.sh"
grep -Fq 'export GOPROXY="file://$inputs/go-cache/cache/download"' "$root/packaging/flatpak/build-offline.sh"

if [ "${PROXOR_FLATPAK_FULL_TEST:-0}" = 1 ]; then
  command -v flatpak-builder >/dev/null
  command -v flatpak >/dev/null
  command -v xvfb-run >/dev/null
  bundle_output="${PROXOR_FLATPAK_BUNDLE_OUTPUT:-}"
  if command -v flatpak-builder-lint >/dev/null; then
    flatpak-builder-lint manifest "$manifest"
  fi
  work="$(mktemp -d)"
  trap 'rm -rf "$work"' EXIT
  flatpak-builder --user --force-clean "$work/online" "$manifest"
  flatpak-builder --user --force-clean --disable-download "$work/offline" "$manifest"
  flatpak build-export "$work/repo" "$work/offline"
  flatpak build-bundle "$work/repo" "$work/io.github.Ogstra.Proxor.flatpak" io.github.Ogstra.Proxor
  if [ -n "$bundle_output" ]; then
    mkdir -p "$(dirname "$bundle_output")"
    cp "$work/io.github.Ogstra.Proxor.flatpak" "$bundle_output"
  fi
  flatpak install --user --noninteractive "$work/io.github.Ogstra.Proxor.flatpak"
  flatpak run --command=sh io.github.Ogstra.Proxor -ceu '
    test "$FLATPAK_ID" = io.github.Ogstra.Proxor
    for asset in geoip.dat geosite.dat geoip.db geosite.db; do test -s "/app/share/proxor/$asset"; done
  '
  set +e
  xvfb-run -a timeout 10s flatpak run io.github.Ogstra.Proxor -many
  result=$?
  set -e
  test "$result" -eq 0 -o "$result" -eq 124
fi
