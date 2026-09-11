#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 1 ] || { echo "Usage: $0 <proxor*.rpm>" >&2; exit 2; }
rpm="$1"; [ -f "$rpm" ] || exit 1
image="fedora@sha256:43b29f65a41eb9c35e1cd5323e3bdf3b655c2357a9f4f1ff2f9c2798e5045d80"
dir="$(CDPATH= cd -- "$(dirname "$rpm")" && pwd)"; name="$(basename "$rpm")"
docker run --rm -v "$dir:/packages:ro" "$image" bash -ceu '
  dnf -y install rpm-build rpmlint desktop-file-utils xvfb xauth
  rpm=/packages/$1; rpm -qpl "$rpm"; rpm -qpR "$rpm"; rpm -qp --scripts "$rpm"; rpmlint "$rpm"
  for p in /usr/bin/proxor /usr/lib/proxor/proxor /usr/lib/proxor/proxor_core /usr/share/proxor/geoip.dat /usr/share/proxor/geosite.dat /usr/share/proxor/geoip.db /usr/share/proxor/geosite.db /usr/share/applications/proxor.desktop /usr/share/icons/hicolor/256x256/apps/proxor.png; do rpm -qpl "$rpm" | grep -qx "$p"; done
  ! rpm -qpl "$rpm" | grep -Eqi "AppDir|linuxdeploy|updater|plugins/|qt[0-9]"; ! rpm -qp --scripts "$rpm" | grep -Eqi "setcap|cap_net_admin"
  dnf -y install "$rpm"; desktop-file-validate /usr/share/applications/proxor.desktop
  set +e; xvfb-run -a timeout 10s /usr/bin/proxor -many; rc=$?; set -e; test "$rc" = 0 -o "$rc" = 124
' bash "$name"
