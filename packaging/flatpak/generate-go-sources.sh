#!/usr/bin/env bash
set -euo pipefail

root="${PROXOR_FLATPAK_SOURCE_ROOT:-$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)}"
output="${PROXOR_FLATPAK_OUTPUT:-$root/packaging/flatpak/generated-go-sources.json}"
check=false
case "${1:-}" in
  '') ;;
  --check) check=true ;;
  *) printf 'Usage: %s [--check]\n' "$0" >&2; exit 2 ;;
esac

generated="$(mktemp)"
trap 'rm -f "$generated"' EXIT

# This is a release-preparation tool, not a Flatpak build command. It resolves every
# module from the locked workspace once, hashes the downloaded source archives, and
# writes only immutable proxy URLs for flatpak-builder to consume offline.
ROOT="$root" OUTPUT="$generated" python3 - <<'PY'
import hashlib
import json
import os
import subprocess

root = os.environ["ROOT"]
modules = {}
for module_dir in ("go/cmd/proxor_core", "go/cmd/updater", "go/grpc_server", "go/proxorlib"):
    raw = subprocess.check_output(
        ["go", "mod", "download", "-json", "all"], cwd=os.path.join(root, module_dir),
        env={**os.environ, "GOWORK": "off"}, text=True)
    decoder = json.JSONDecoder()
    pos = 0
    while pos < len(raw):
        while pos < len(raw) and raw[pos].isspace():
            pos += 1
        if pos >= len(raw):
            break
        item, pos = decoder.raw_decode(raw, pos)
        if not item.get("Version") or not item.get("Zip"):
            continue
        modules[(item["Path"], item["Version"])] = item

def escape(value):
    return "".join("!" + char.lower() if "A" <= char <= "Z" else char for char in value)

sources = [{
    "kind": "proxor-recursive-source",
    "type": "archive",
    "url": "https://github.com/Ogstra/proxor/releases/download/v${VERSION}/proxor-${VERSION}.tar.gz",
    "sha256": "${PROXOR_SOURCE_SHA256}",
    "manifest": "proxor-${VERSION}.source-manifest",
    "submodules": ["3rdparty/sing-box", "3rdparty/QHotkey", "3rdparty/SQLiteCpp"],
}]
for path, version in sorted(modules):
    item = modules[(path, version)]
    with open(item["Zip"], "rb") as archive:
        sha256 = hashlib.sha256(archive.read()).hexdigest()
    sources.append({
        "kind": "go-module", "type": "archive",
        "module": path, "version": version,
        "url": f"https://proxy.golang.org/{escape(path)}/@v/{escape(version)}.zip",
        "sha256": sha256,
    })

document = {
    "schema": 1,
    "workspace": "go.work",
    "generator": {
        "name": "go module proxy archive closure",
        "revision": "go-1.26-workspace",
        "sha256": hashlib.sha256(open(os.path.join(root, "go.work"), "rb").read()).hexdigest(),
    },
    "sources": sources,
}
with open(os.environ["OUTPUT"], "w", encoding="utf-8") as result:
    json.dump(document, result, indent=2, sort_keys=True)
    result.write("\n")
PY

if "$check"; then
  cmp "$generated" "$output"
else
  mv "$generated" "$output"
  trap - EXIT
fi
