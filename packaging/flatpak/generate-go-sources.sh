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
import urllib.error
import urllib.request

root = os.environ["ROOT"]
module_dirs = ("go/cmd/proxor_core", "go/cmd/updater", "go/grpc_server", "go/proxorlib")
offline_env = {**os.environ, "GOWORK": "off"}

modules = {}
graph = {}
for module_dir in module_dirs:
    cwd = os.path.join(root, module_dir)
    raw = subprocess.check_output(
        ["go", "mod", "download", "-json", "all"], cwd=cwd, env=offline_env, text=True)
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

# Minimal version selection reads the .mod of every version the checksum database
# locked, not only of the versions that end up linked in, so the offline proxy has
# to serve all of them. go.work.sum covers the workspace build, the per-module
# go.sum files cover a single-module build of the same sources.
for sum_file in ("go.work.sum",) + tuple(d + "/go.sum" for d in module_dirs):
    path_name = os.path.join(root, sum_file)
    if not os.path.exists(path_name):
        continue
    with open(path_name, encoding="utf-8") as checksums:
        for line in checksums:
            fields = line.split()
            if len(fields) == 3 and fields[1].endswith("/go.mod"):
                graph[(fields[0], fields[1][: -len("/go.mod")])] = None

def escape(value):
    return "".join("!" + char.lower() if "A" <= char <= "Z" else char for char in value)

for path, version in sorted(graph):
    url = f"https://proxy.golang.org/{escape(path)}/@v/{escape(version)}.mod"
    try:
        with urllib.request.urlopen(url) as response:
            graph[(path, version)] = hashlib.sha256(response.read()).hexdigest()
    except urllib.error.HTTPError as error:
        # A locked version the proxy does not serve cannot be part of an offline
        # build either: a replace directive resolves it from the source tree.
        if error.code != 404:
            raise
        del graph[(path, version)]

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
for path, version in sorted(graph):
    sources.append({
        "kind": "go-module-requirements", "type": "file",
        "module": path, "version": version,
        "url": f"https://proxy.golang.org/{escape(path)}/@v/{escape(version)}.mod",
        "sha256": graph[(path, version)],
    })

document = {
    "schema": 1,
    "workspace": "go.work",
    "generator": {
        "name": "go module proxy archive closure",
        "revision": "go-1.26-workspace",
        "sha256": hashlib.sha256(open(os.path.join(root, "go.work"), "rb").read().replace(b"\r\n", b"\n")).hexdigest(),
    },
    "sources": sources,
}
with open(os.environ["OUTPUT"], "w", encoding="utf-8") as result:
    json.dump(document, result, indent=2, sort_keys=True)
    result.write("\n")
PY

if "$check"; then
  if ! cmp -s "$generated" "$output"; then
    diff -u "$output" "$generated" >&2 || true
    exit 1
  fi
else
  mv "$generated" "$output"
  trap - EXIT
fi
