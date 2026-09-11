#!/usr/bin/env bash
# Run from a Linux CI runner. Docker is deliberately the only privilege boundary.
set -euo pipefail

usage() { printf 'Usage: %s <proxor_*.deb>\n' "$0" >&2; exit 2; }
[ "$#" = 1 ] || usage
deb="$1"
[ -f "$deb" ] || { printf 'package not found: %s\n' "$deb" >&2; exit 1; }
case "$(basename "$deb")" in proxor_[0-9]*.[0-9]*.[0-9]*-1_amd64.deb) ;; *) usage ;; esac

image="debian@sha256:88200866dfff7ea7f5cbcb6ec7c8a701889efe6fe859fe64d6990e4b07ea4171"
package_dir="$(CDPATH= cd -- "$(dirname -- "$deb")" && pwd)"
package_name="$(basename "$deb")"

docker run --rm -v "$package_dir:/packages:ro" "$image" bash -ceu '
  export DEBIAN_FRONTEND=noninteractive
  apt-get update
  apt-get install -y --no-install-recommends desktop-file-utils lintian xvfb xauth \
    libdbus-1-3 libegl1 libfontconfig1 libfreetype6 libgl1 libopengl0 libxkbcommon-x11-0 \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-xinerama0 libxcb-xkb1
  deb="/packages/$1"
  dpkg-deb --info "$deb"
  dpkg-deb --contents "$deb"
  lintian --fail-on error "$deb"
  contents="$(dpkg-deb --contents "$deb")"
  for path in usr/bin/proxor usr/lib/proxor/proxor usr/lib/proxor/proxor_core \
      usr/share/proxor/geoip.dat usr/share/proxor/geosite.dat \
      usr/share/proxor/geoip.db usr/share/proxor/geosite.db \
      usr/share/applications/proxor.desktop usr/share/icons/hicolor/256x256/apps/proxor.png; do
    printf "%s\n" "$contents" | grep -Eq "[[:space:]]\./$path$"
  done
  ! printf "%s\n" "$contents" | grep -Eqi "AppDir|linuxdeploy|updater|plugins/|qt[0-9]"
  control="$(dpkg-deb --control "$deb" /tmp/proxor-control; cat /tmp/proxor-control/postinst 2>/dev/null || true)"
  ! printf "%s\n" "$control" | grep -Eqi "setcap|cap_net_admin"
  apt-get install -y "$deb"
  desktop-file-validate /usr/share/applications/proxor.desktop
  set +e
  xvfb-run -a timeout 10s /usr/bin/proxor -many
  rc=$?
  set -e
  test "$rc" -eq 0 -o "$rc" -eq 124
' bash "$package_name"
