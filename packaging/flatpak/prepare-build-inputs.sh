#!/usr/bin/env bash
set -euo pipefail

usage() {
  printf 'Usage: %s --source-archive <tar.gz> --source-manifest <manifest> --geodata <directory> --output <directory>\n' "$0" >&2
  exit 2
}

archive= manifest= geodata= output=
while [ "$#" -gt 0 ]; do
  case "$1" in
    --source-archive) archive="${2:-}"; shift 2 ;;
    --source-manifest) manifest="${2:-}"; shift 2 ;;
    --geodata) geodata="${2:-}"; shift 2 ;;
    --output) output="${2:-}"; shift 2 ;;
    *) usage ;;
  esac
done
test -f "$archive" && test -f "$manifest" && test -d "$geodata" && test -n "$output" || usage

root="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"
sources="$root/packaging/flatpak/generated-go-sources.json"
version="$(tr -d '\n' < "$root/VERSION.txt")"
sha="$(shasum -a 256 "$archive" | awk '{print $1}')"
grep -qx "version=$version" "$manifest"
grep -qx "sha256=$sha" "$manifest"
for submodule in 3rdparty_sing-box 3rdparty_QHotkey 3rdparty_SQLiteCpp; do
  grep -Eq "^submodule\.$submodule=[0-9a-f]{40}$" "$manifest"
done

stage="$(mktemp -d "$(dirname "$output")/.flatpak-input.XXXXXX")"
trap 'rm -rf "$stage"' EXIT
mkdir -p "$stage/go-cache/cache/download" "$stage/geodata"
cp "$archive" "$stage/proxor-source.tar.gz"
for asset in geoip.dat geosite.dat geoip.db geosite.db; do
  test -s "$geodata/$asset"
  cp "$geodata/$asset" "$stage/geodata/$asset"
done

python3 - "$sources" "$stage/go-cache/cache/download" <<'PY'
import hashlib
import json
import os
import re
import sys
import urllib.request

document = json.load(open(sys.argv[1], encoding="utf-8"))
cache = sys.argv[2]
for source in document["sources"]:
    if source.get("kind") != "go-module":
        continue
    path = re.sub(r"[A-Z]", lambda match: "!" + match.group(0).lower(), source["module"])
    version = re.sub(r"[A-Z]", lambda match: "!" + match.group(0).lower(), source["version"])
    destination = os.path.join(cache, path, "@v", version + ".zip")
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    with urllib.request.urlopen(source["url"]) as response, open(destination, "wb") as archive:
        archive.write(response.read())
    actual = hashlib.sha256(open(destination, "rb").read()).hexdigest()
    if actual != source["sha256"]:
        raise SystemExit("checksum mismatch for " + source["module"])
PY

rm -rf "$output"
mv "$stage" "$output"
trap - EXIT
