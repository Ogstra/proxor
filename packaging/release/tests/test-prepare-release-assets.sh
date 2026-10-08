#!/usr/bin/env bash
set -euo pipefail
script="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)/prepare-release-assets.sh"
tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT; mkdir -p "$tmp/in/release-assets-source"
printf x > "$tmp/in/release-assets-source/proxor-1.2.3.tar.gz"; sha="$(shasum -a 256 "$tmp/in/release-assets-source/proxor-1.2.3.tar.gz"|awk '{print $1}')"
printf 'version=1.2.3\nsha256=%s\n' "$sha" > "$tmp/in/release-assets-source/proxor-1.2.3.source-manifest"
"$script" prepare-source-release --input "$tmp/in" --version 1.2.3 --output "$tmp/out"; test -f "$tmp/out/proxor-1.2.3.tar.gz"
! "$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final" --public-source-url 'https://invalid.example/proxor-1.2.3.tar.gz'

url="https://github.com/Ogstra/proxor/releases/download/v1.2.3/proxor-1.2.3.tar.gz"
mkdir -p "$tmp/in/release-assets-arch"
printf 'source=("proxor-1.2.3.tar.gz::%s")\nsha256sums=(%s)\n' "$url" "$sha" > "$tmp/in/release-assets-arch/PKGBUILD"
printf '\tsource = proxor-1.2.3.tar.gz::%s\n\tsha256sums = %s\n' "$url" "$sha" > "$tmp/in/release-assets-arch/.SRCINFO"
for asset in proxor-1.2.3-windows64.zip proxor-1.2.3-winget-x64.zip proxor-1.2.3-macos-arm64.zip proxor-1.2.3-symbols.zip proxor-1.2.3.AppImage \
  proxor-1.2.3.deb proxor-1.2.3.rpm; do
  mkdir -p "$tmp/in/$asset.d"; printf '%s' "$asset" > "$tmp/in/$asset.d/$asset"
done
# Flatpak is deprecated (it cannot run Tun mode): it is neither required nor shipped, even if a stray bundle is present.
mkdir -p "$tmp/in/stray-flatpak"; printf x > "$tmp/in/stray-flatpak/proxor-1.2.3.flatpak"
# Per-job build artifacts share names across jobs and never become release assets.
for job in linux windows; do
  mkdir -p "$tmp/in/$job"; printf x > "$tmp/in/$job/artifacts.tgz"
done
# Debug symbols and debug packages are build artifacts, not release downloads.
mkdir -p "$tmp/in/rpms"
for debug in proxor-debuginfo-1.2.3-1.fc44.x86_64.rpm proxor-debugsource-1.2.3-1.fc44.x86_64.rpm; do
  printf x > "$tmp/in/rpms/$debug"
done
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final" --public-source-url "$url"
for excluded in proxor-debuginfo-1.2.3-1.fc44.x86_64.rpm proxor-debugsource-1.2.3-1.fc44.x86_64.rpm \
  proxor-1.2.3-symbols.zip proxor-1.2.3.source-manifest; do
  test ! -e "$tmp/final/$excluded"
done
test -f "$tmp/final/recipes/proxor-1.2.3.source-manifest"
test ! -e "$tmp/final/artifacts.tgz"
# Packaging recipes are staged for their package repositories, not for the release.
test -f "$tmp/final/recipes/aur/PKGBUILD"
test -f "$tmp/final/recipes/aur/.SRCINFO"
test -f "$tmp/final/recipes/winget-manifests/Ogstra.Proxor.yaml"
test ! -e "$tmp/final/aur"
test ! -e "$tmp/final/winget-manifests"
test ! -e "$tmp/final/proxor-1.2.3.flatpak"
! grep -q 'flatpak' "$tmp/final/SHA256SUMS"
test -f "$tmp/final/proxor-1.2.3-macos-arm64.zip"
grep -q 'proxor-1.2.3-macos-arm64.zip' "$tmp/final/SHA256SUMS"
! grep -q recipes "$tmp/final/SHA256SUMS"

# A release without the macOS zip would leave the Homebrew cask pointing at nothing.
mv "$tmp/in/proxor-1.2.3-macos-arm64.zip.d" "$tmp/macos-aside"
if err="$("$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-no-macos" --public-source-url "$url" 2>&1)"; then
  echo 'a release without the macOS zip must be refused' >&2; exit 1
fi
grep -qF 'missing *-macos-arm64.zip' <<<"$err"
mv "$tmp/macos-aside" "$tmp/in/proxor-1.2.3-macos-arm64.zip.d"

# Two release assets with the same name would silently overwrite each other.
mkdir -p "$tmp/in/duplicate"; printf y > "$tmp/in/duplicate/proxor-1.2.3.deb"
! "$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-duplicate" --public-source-url "$url"

# Windows-only stable release: only the Windows downloads and a checksum list that matches them.
rm -rf "$tmp/in/duplicate"
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-full" --public-source-url "$url"
full_sums="$(cat "$tmp/final-full/SHA256SUMS")"
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-win" --public-source-url "$url" --windows-only
actual="$(cd "$tmp/final-win" && find . -maxdepth 1 -type f ! -name proxor-1.2.3.tar.gz | sed 's|^\./||' | sort | tr '\n' ' ')"
test "$actual" = 'SHA256SUMS proxor-1.2.3-windows64.zip proxor-1.2.3-winget-x64.zip ' || { echo "unexpected windows-only files: $actual" >&2; exit 1; }
# The source tarball is already attached to the release, as in the full path, so it is listed too.
sums_names="$(awk '{print $2}' "$tmp/final-win/SHA256SUMS" | sed 's|^\./||' | sort | tr '\n' ' ')"
test "$sums_names" = 'proxor-1.2.3-windows64.zip proxor-1.2.3-winget-x64.zip proxor-1.2.3.tar.gz ' || { echo "unexpected windows-only SHA256SUMS: $sums_names" >&2; exit 1; }
(cd "$tmp/final-win" && shasum -a 256 -c SHA256SUMS >/dev/null)
test -f "$tmp/final-win/proxor-1.2.3.tar.gz"
test -f "$tmp/final-win/recipes/winget-manifests/Ogstra.Proxor.yaml"
test ! -e "$tmp/final-win/recipes/aur"
# The full path is unaffected by the flag existing: same checksum list as before.
grep -q 'proxor-1.2.3-macos-arm64.zip' <<<"$full_sums"
! grep -q 'flatpak' <<<"$full_sums"
# The flag is a final-assets option only, and the Windows zips remain mandatory.
! "$script" prepare-source-release --input "$tmp/in" --version 1.2.3 --output "$tmp/src-win" --windows-only
mv "$tmp/in/proxor-1.2.3-windows64.zip.d" "$tmp/win-aside"
if err="$("$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-win-missing" --public-source-url "$url" --windows-only 2>&1)"; then
  echo 'a Windows-only release without the Windows zip must be refused' >&2; exit 1
fi
grep -qF 'missing *-windows64.zip' <<<"$err"
mv "$tmp/win-aside" "$tmp/in/proxor-1.2.3-windows64.zip.d"

# Without any Flatpak bundle the full release is still complete.
rm -rf "$tmp/in/stray-flatpak"
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-no-flatpak" --public-source-url "$url"
test ! -e "$tmp/final-no-flatpak/proxor-1.2.3.flatpak"

# The macOS Intel zip is optional: absent -> same release as before; present -> shipped and checksummed;
# --require-macos-intel refuses a release without it. It is a final-assets option and never part of --windows-only.
intel=proxor-1.2.3-macos-x86_64.zip
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-no-intel" --public-source-url "$url"
test ! -e "$tmp/final-no-intel/$intel"
! grep -q 'macos-x86_64' "$tmp/final-no-intel/SHA256SUMS"
if err="$("$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-req-missing" --public-source-url "$url" --require-macos-intel 2>&1)"; then
  echo 'a release without the Intel zip must be refused with --require-macos-intel' >&2; exit 1
fi
grep -qF 'missing *-macos-x86_64.zip' <<<"$err"
mkdir -p "$tmp/in/$intel.d"; printf '%s' "$intel" > "$tmp/in/$intel.d/$intel"
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-intel" --public-source-url "$url"
test -f "$tmp/final-intel/$intel"
grep -q "$intel" "$tmp/final-intel/SHA256SUMS"
(cd "$tmp/final-intel" && shasum -a 256 -c SHA256SUMS >/dev/null)
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-intel-req" --public-source-url "$url" --require-macos-intel
test -f "$tmp/final-intel-req/$intel"
"$script" prepare-final-assets --input "$tmp/in" --version 1.2.3 --output "$tmp/final-win-intel" --public-source-url "$url" --windows-only
test ! -e "$tmp/final-win-intel/$intel"
! grep -q 'macos-x86_64' "$tmp/final-win-intel/SHA256SUMS"
! "$script" prepare-source-release --input "$tmp/in" --version 1.2.3 --output "$tmp/src-intel" --require-macos-intel
