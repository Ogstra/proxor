#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf '%s\n' "Usage: DESTDIR=<directory> $0 --gui <path> --core <path> --geodata <directory>" >&2
    exit 2
}

test -n "${DESTDIR:-}" || usage

gui=""
core=""
geodata=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --gui)
            [ "$#" -ge 2 ] || usage
            gui="$2"
            shift 2
            ;;
        --core)
            [ "$#" -ge 2 ] || usage
            core="$2"
            shift 2
            ;;
        --geodata)
            [ "$#" -ge 2 ] || usage
            geodata="$2"
            shift 2
            ;;
        *)
            usage
            ;;
    esac
done

[ -n "$gui" ] && [ -n "$core" ] && [ -n "$geodata" ] || usage
[ -f "$gui" ] || { printf 'GUI not found: %s\n' "$gui" >&2; exit 1; }
[ -f "$core" ] || { printf 'core not found: %s\n' "$core" >&2; exit 1; }
[ -d "$geodata" ] || { printf 'geodata directory not found: %s\n' "$geodata" >&2; exit 1; }

for asset in geoip.dat geosite.dat geoip.db geosite.db; do
    [ -s "$geodata/$asset" ] || { printf 'required geodata not found: %s\n' "$geodata/$asset" >&2; exit 1; }
done

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
wrapper="$script_dir/proxor-wrapper.sh"
[ -f "$wrapper" ] || { printf 'wrapper not found: %s\n' "$wrapper" >&2; exit 1; }

parent="$(dirname "$DESTDIR")"
mkdir -p "$parent"
stage="$(mktemp -d "${parent}/.$(basename "$DESTDIR").tmp.XXXXXX")"
trap 'rm -rf "$stage"' EXIT

mkdir -p "$stage/usr/bin" "$stage/usr/lib/proxor" "$stage/usr/share/proxor"
install -m 0755 "$wrapper" "$stage/usr/bin/proxor"
install -m 0755 "$gui" "$stage/usr/lib/proxor/proxor"
install -m 0755 "$core" "$stage/usr/lib/proxor/proxor_core"
for asset in geoip.dat geosite.dat geoip.db geosite.db; do
    install -m 0644 "$geodata/$asset" "$stage/usr/share/proxor/$asset"
done

rm -rf "$DESTDIR"
mv "$stage" "$DESTDIR"
trap - EXIT
