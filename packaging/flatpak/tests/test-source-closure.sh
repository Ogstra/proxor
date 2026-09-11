#!/usr/bin/env bash
set -euo pipefail

root="$(CDPATH= cd -- "$(dirname "$0")/../../.." && pwd)"
desktop="$root/assets/linux/io.github.Ogstra.Proxor.desktop"
metainfo="$root/assets/linux/io.github.Ogstra.Proxor.metainfo.xml"
sources="$root/packaging/flatpak/generated-go-sources.json"
generator="$root/packaging/flatpak/generate-go-sources.sh"

test -f "$desktop"
test -f "$metainfo"
test -x "$generator"
grep -qx 'Exec=proxor' "$desktop"
grep -qx 'Icon=io.github.Ogstra.Proxor' "$desktop"
grep -q '<id>io.github.Ogstra.Proxor</id>' "$metainfo"
grep -qi 'TUN.*unavailable\|unavailable.*TUN' "$metainfo"
grep -qi 'system proxy.*unavailable\|unavailable.*system proxy' "$metainfo"

python3 - "$sources" <<'PY'
import json
import re
import sys

with open(sys.argv[1], encoding="utf-8") as source_file:
    document = json.load(source_file)

assert document["schema"] == 1
assert document["workspace"] == "go.work"
assert document["generator"]["revision"]
assert re.fullmatch(r"[0-9a-f]{64}", document["generator"]["sha256"])
assert document["sources"], "source closure is empty"

source_archive = False
for source in document["sources"]:
    assert source["type"] == "archive"
    assert source["url"].startswith("https://")
    assert not re.search(r"/(?:main|master)(?:/|$)", source["url"])
    if source.get("kind") == "proxor-recursive-source":
        assert source["sha256"] == "${PROXOR_SOURCE_SHA256}"
        assert source["manifest"] == "proxor-${VERSION}.source-manifest"
        assert set(source["submodules"]) == {
            "3rdparty/sing-box", "3rdparty/QHotkey", "3rdparty/SQLiteCpp"
        }
        source_archive = True
    else:
        assert re.fullmatch(r"[0-9a-f]{64}", source["sha256"])

assert source_archive, "the recursive Plan 12 source archive is required"
PY
