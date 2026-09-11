#!/usr/bin/env bash
set -euo pipefail

usage() {
    printf '%s\n' "Usage: $0 --version X.Y.Z --url https://github.com/Ogstra/proxor/releases/download/vX.Y.Z/proxor-X.Y.Z-winget-x64.zip --sha256 <64-hex> --output <directory>" >&2
    exit 2
}

version=""
url=""
sha256=""
output=""
while [ "$#" -gt 0 ]; do
    case "$1" in
        --version) version="${2:-}"; shift 2 ;;
        --url) url="${2:-}"; shift 2 ;;
        --sha256) sha256="${2:-}"; shift 2 ;;
        --output) output="${2:-}"; shift 2 ;;
        *) usage ;;
    esac
done

[ -n "$version" ] && [ -n "$url" ] && [ -n "$sha256" ] && [ -n "$output" ] || usage
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { printf '%s\n' 'version must be X.Y.Z' >&2; exit 1; }
expected_url="https://github.com/Ogstra/proxor/releases/download/v${version}/proxor-${version}-winget-x64.zip"
[ "$url" = "$expected_url" ] || { printf '%s\n' 'URL must be the HTTPS GitHub Release winget asset for the supplied version' >&2; exit 1; }
[[ "$sha256" =~ ^[[:xdigit:]]{64}$ ]] || { printf '%s\n' 'SHA-256 must contain exactly 64 hexadecimal characters' >&2; exit 1; }
sha256="$(printf '%s' "$sha256" | tr '[:lower:]' '[:upper:]')"

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
templates="$repo_root/packaging/winget/templates"
for template in Ogstra.Proxor.yaml.in Ogstra.Proxor.locale.en-US.yaml.in Ogstra.Proxor.installer.yaml.in; do
    [ -f "$templates/$template" ] || { printf 'missing template: %s\n' "$template" >&2; exit 1; }
done

output_parent="$(dirname "$output")"
mkdir -p "$output_parent"
stage="$(mktemp -d "${output_parent}/.$(basename "$output").tmp.XXXXXX")"
trap 'rm -rf "$stage"' EXIT

render() {
    local input="$1"
    local destination="$2"
    sed -e "s|@VERSION@|$version|g" -e "s|@URL@|$url|g" -e "s|@SHA256@|$sha256|g" "$input" > "$destination"
}

render "$templates/Ogstra.Proxor.yaml.in" "$stage/Ogstra.Proxor.yaml"
render "$templates/Ogstra.Proxor.locale.en-US.yaml.in" "$stage/Ogstra.Proxor.locale.en-US.yaml"
render "$templates/Ogstra.Proxor.installer.yaml.in" "$stage/Ogstra.Proxor.installer.yaml"

rm -rf "$output"
mv "$stage" "$output"
trap - EXIT
