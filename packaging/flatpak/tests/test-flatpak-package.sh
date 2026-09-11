#!/usr/bin/env bash
set -euo pipefail

root="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)"
manifest="$root/packaging/flatpak/io.github.Ogstra.Proxor.yml"
wrapper="$root/packaging/flatpak/proxor-wrapper.sh"

test -f "$manifest"
test -x "$wrapper"
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
grep -Fq '/app/share/proxor/geoip.dat' "$root/packaging/flatpak/build-offline.sh"
grep -Fq 'FLATPAK_ID=io.github.Ogstra.Proxor' "$manifest"

if [ "${PROXOR_FLATPAK_FULL_TEST:-0}" = 1 ]; then
  command -v flatpak-builder >/dev/null
  command -v flatpak >/dev/null
  command -v xvfb-run >/dev/null
  flatpak-builder-lint manifest "$manifest"
  work="$(mktemp -d)"
  trap 'rm -rf "$work"' EXIT
  flatpak-builder --user --force-clean "$work/online" "$manifest"
  flatpak-builder --user --force-clean --disable-download "$work/offline" "$manifest"
  flatpak build-export "$work/repo" "$work/offline"
  flatpak build-bundle "$work/repo" "$work/io.github.Ogstra.Proxor.flatpak" io.github.Ogstra.Proxor
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
