#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 1 ] || exit 2; root="$1"; test -f "$root/PKGBUILD"; test -f "$root/.SRCINFO"
grep -qx 'pkgname = proxor' "$root/.SRCINFO"; ! grep -Eqi 'SKIP|master|main|proxor-bin' "$root/PKGBUILD"; cmp "$root/.SRCINFO" <(cd "$root" && makepkg --printsrcinfo)
makepkg --syncdeps --cleanbuild --noconfirm -D "$root"; namcap "$root/PKGBUILD" "$root"/*.pkg.tar.*
