#!/usr/bin/env bash
set -euo pipefail

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"
helper="$repo_root/packaging/linux/stage-native-root.sh"

work="$(mktemp -d "${TMPDIR:-/tmp}/proxor-native-stage.XXXXXX")"
trap 'rm -rf "$work"' EXIT

fixtures="$work/fixtures"
mkdir -p "$fixtures/geodata"
printf 'gui\n' > "$fixtures/proxor"
printf 'core\n' > "$fixtures/proxor_core"
chmod +x "$fixtures/proxor" "$fixtures/proxor_core"
for asset in geoip.dat geosite.dat geoip.db geosite.db; do
    printf '%s\n' "$asset" > "$fixtures/geodata/$asset"
done

stage="$work/stage"
DESTDIR="$stage" "$helper" --gui "$fixtures/proxor" --core "$fixtures/proxor_core" --geodata "$fixtures/geodata" --channel deb

test -x "$stage/usr/bin/proxor"
test -x "$stage/usr/lib/proxor/proxor"
test -x "$stage/usr/lib/proxor/proxor_core"
grep -Fx 'exec /usr/lib/proxor/proxor "$@"' "$stage/usr/bin/proxor"
for asset in geoip.dat geosite.dat geoip.db geosite.db; do
    test -s "$stage/usr/share/proxor/$asset"
    cmp "$fixtures/geodata/$asset" "$stage/usr/share/proxor/$asset"
done

marker="$stage/usr/share/proxor/package-channel"
test -f "$marker"
test "$(stat -c '%a' "$marker" 2>/dev/null || stat -f '%Lp' "$marker")" = "644"
test "$(cat "$marker")" = "deb"
printf 'deb\n' | cmp - "$marker"

test "$(find "$stage" -type f | wc -l | tr -d ' ')" -eq 8
test ! -e "$stage/AppDir"
test ! -e "$stage/linuxdeploy"
test ! -e "$stage/usr/lib/proxor/updater"
test ! -e "$stage/usr/lib/proxor/plugins"

before_failure="$(find "$stage" -print | LC_ALL=C sort)"
rm "$fixtures/geodata/geosite.db"
if DESTDIR="$stage" "$helper" --gui "$fixtures/proxor" --core "$fixtures/proxor_core" --geodata "$fixtures/geodata" --channel deb; then
    printf '%s\n' 'expected missing geodata to fail' >&2
    exit 1
fi
after_failure="$(find "$stage" -print | LC_ALL=C sort)"
test "$before_failure" = "$after_failure"

if DESTDIR="$work/missing" "$helper" --gui "$work/absent" --core "$fixtures/proxor_core" --geodata "$fixtures/geodata" --channel deb; then
    printf '%s\n' 'expected missing GUI to fail' >&2
    exit 1
fi
test ! -e "$work/missing"

# --channel is required: omitting it must fail before any staging happens.
if DESTDIR="$work/missing-channel" "$helper" --gui "$fixtures/proxor" --core "$fixtures/proxor_core" --geodata "$fixtures/geodata"; then
    printf '%s\n' 'expected missing --channel to fail' >&2
    exit 1
fi
test ! -e "$work/missing-channel"

# Only deb, rpm and arch are accepted.
if DESTDIR="$work/bad-channel" "$helper" --gui "$fixtures/proxor" --core "$fixtures/proxor_core" --geodata "$fixtures/geodata" --channel nonsense; then
    printf '%s\n' 'expected --channel nonsense to fail' >&2
    exit 1
fi
test ! -e "$work/bad-channel"
