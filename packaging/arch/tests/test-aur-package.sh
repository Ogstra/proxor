#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 1 ] || exit 2; root="$1"; test -f "$root/PKGBUILD"; test -f "$root/.SRCINFO"
grep -qx 'pkgname = proxor' "$root/.SRCINFO"; ! grep -Eqi 'SKIP|master|main|proxor-bin' "$root/PKGBUILD"; cmp "$root/.SRCINFO" <(cd "$root" && makepkg --printsrcinfo)
# AUR lists the maintainer from this comment, and the icon theme dependency is the one
# namcap reports as detected but missing.
grep -Eq '^# Maintainer: .+ <.+>$' "$root/PKGBUILD"; grep -q 'hicolor-icon-theme' "$root/PKGBUILD"
makepkg --syncdeps --cleanbuild --noconfirm -D "$root"; namcap "$root/PKGBUILD" "$root"/*.pkg.tar.*
# This test neither installs nor launches the package, so package content -- not the
# installed app's log -- is as far as the channel marker can be verified here.
bsdtar -xOf "$root"/*.pkg.tar.* usr/share/proxor/package-channel | grep -qx arch
