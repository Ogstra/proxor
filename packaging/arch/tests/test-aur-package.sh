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
# makepkg also writes a proxor-debug package, so the archive is named explicitly rather
# than globbed, and the content is compared without a pipe: grep -q would close it and
# bsdtar would die of SIGPIPE, which pipefail reports as a failure.
pkg="$(ls "$root"/proxor-[0-9]*.pkg.tar.* | grep -v -- '-debug-' | head -n1)"
test -n "$pkg"
channel="$(bsdtar -xOf "$pkg" usr/share/proxor/package-channel)"
test "$channel" = arch
