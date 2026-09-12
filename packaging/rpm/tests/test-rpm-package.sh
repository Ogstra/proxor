#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 1 ] || { echo "Usage: $0 <proxor*.rpm>" >&2; exit 2; }
rpm="$1"; [ -f "$rpm" ] || exit 1
# A release tag repeated in the file name, as in 1.6.5-1.fc44.fc44, means the spec and
# the platform both appended the dist tag.
basename "$rpm" | grep -Eq '^proxor-[0-9]+\.[0-9]+\.[0-9]+-[0-9]+\.fc[0-9]+\.x86_64\.rpm$' || {
  echo "unexpected release tag in $(basename "$rpm")" >&2; exit 1; }
image="fedora@sha256:43b29f65a41eb9c35e1cd5323e3bdf3b655c2357a9f4f1ff2f9c2798e5045d80"
dir="$(CDPATH= cd -- "$(dirname "$rpm")" && pwd)"; name="$(basename "$rpm")"
config="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)/proxor.rpmlint.toml"
[ -f "$config" ] || exit 1
docker run --rm -v "$dir:/packages:ro" -v "$config:/rpmlint/proxor.rpmlint.toml:ro" "$image" bash -ceu '
  dnf -y install rpm-build rpmlint desktop-file-utils xorg-x11-server-Xvfb xauth
  rpm=/packages/$1; rpm -qpl "$rpm"; rpm -qpR "$rpm"; rpm -qp --scripts "$rpm"; rpmlint --config /rpmlint/proxor.rpmlint.toml "$rpm"
  for p in /usr/bin/proxor /usr/lib/proxor/proxor /usr/lib/proxor/proxor_core /usr/share/proxor/geoip.dat /usr/share/proxor/geosite.dat /usr/share/proxor/geoip.db /usr/share/proxor/geosite.db /usr/share/applications/proxor.desktop /usr/share/icons/hicolor/256x256/apps/proxor.png; do rpm -qpl "$rpm" | grep -qx "$p"; done
  ! rpm -qpl "$rpm" | grep -Eqi "AppDir|linuxdeploy|updater|plugins/|qt[0-9]"; ! rpm -qp --scripts "$rpm" | grep -Eqi "setcap|cap_net_admin"
  dnf -y install "$rpm"; desktop-file-validate /usr/share/applications/proxor.desktop
  grep -q QT_PLUGIN_PATH /usr/bin/proxor
  # The icons are SVG, so the reader plugins have to come with the dependencies. Their
  # directory differs between distributions, hence the search and the listing on failure.
  for plugin in libqsvgicon.so libqsvg.so; do
    find /usr/lib64/qt6/plugins /usr/lib/qt6/plugins -name "$plugin" 2>/dev/null | grep -q . || {
      echo "missing Qt plugin $plugin"; find /usr/lib64/qt6/plugins /usr/lib/qt6/plugins -name '*.so' 2>/dev/null | sort; exit 1; }
  done
  set +e; xvfb-run -a timeout 10s /usr/bin/proxor -many; rc=$?; set -e; test "$rc" = 0 -o "$rc" = 124
' bash "$name"
