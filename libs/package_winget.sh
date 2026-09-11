#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -ne 2 ]; then
    printf '%s\n' "Usage: $0 <deployed-windows-dir> <output-winget-x64.zip>" >&2
    exit 2
fi

source_dir="$1"
output_zip="$2"
case "$(basename "$output_zip")" in
    *-winget-x64.zip) ;;
    *)
        printf '%s\n' 'winget package filename must end in -winget-x64.zip' >&2
        exit 1
        ;;
esac

for required in \
    "$source_dir/proxor.exe" \
    "$source_dir/proxor_core.exe" \
    "$source_dir/config/runtime/Qt6Core.dll" \
    "$source_dir/config/runtime/plugins/platforms/qwindows.dll"; do
    [ -f "$required" ] || {
        printf 'missing required Windows payload: %s\n' "$required" >&2
        exit 1
    }
done

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
stage="$(mktemp -d "${TMPDIR:-/tmp}/proxor-winget-stage.XXXXXX")"
trap 'rm -rf "$stage"' EXIT

cp -a "$source_dir/." "$stage/proxor"
mkdir -p "$stage/proxor/config/package-manager"
printf '%s\n' 'winget-managed-install' > "$stage/proxor/config/package-manager/winget"

"$repo_root/libs/package_release.sh" "$stage/proxor" "$output_zip"
