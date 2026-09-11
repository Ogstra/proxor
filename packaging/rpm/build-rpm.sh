#!/usr/bin/env bash
set -euo pipefail
[ "$#" = 2 ] && [ "$1" = --source-archive ] || { echo "Usage: $0 --source-archive <proxor-X.Y.Z.tar.gz>" >&2; exit 2; }
archive="$2"; [ -f "$archive" ] || exit 1
version="$(basename "$archive" | sed -E 's/^proxor-([0-9]+\.[0-9]+\.[0-9]+)\.tar\.gz$/\1/')"
case "$version" in [0-9]*.[0-9]*.[0-9]*) ;; *) exit 1;; esac
root="${RPMBUILD_ROOT:-$PWD/rpmbuild}"; rm -rf "$root"; mkdir -p "$root"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}
cp "$archive" "$root/SOURCES/proxor-$version.tar.gz"; cp packaging/rpm/proxor.spec "$root/SPECS/"
rpmbuild -ba "$root/SPECS/proxor.spec" --define "_topdir $root" --define "_sourcedir $root/SOURCES" --define "_specdir $root/SPECS" --define "_builddir $root/BUILD" --define "_rpmdir $root/RPMS" --define "version $version" --define "release 1"
