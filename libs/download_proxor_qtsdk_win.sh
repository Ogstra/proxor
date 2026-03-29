#!/bin/bash
set -euo pipefail

: "${PROXOR_QT_RUNTIME_REPO:=Ogstra/proxor}"
: "${PROXOR_QT_RUNTIME_TAG:=qt-runtime-2026-03-23}"

mkdir -p qtsdk
cd qtsdk

download_asset() {
  local asset="$1"
  local url="https://github.com/${PROXOR_QT_RUNTIME_REPO}/releases/download/${PROXOR_QT_RUNTIME_TAG}/${asset}"

  if gh release download "${PROXOR_QT_RUNTIME_TAG}" -R "${PROXOR_QT_RUNTIME_REPO}" -p "${asset}"; then
    DOWNLOADED_ASSET="$asset"
    return 0
  fi

  echo "Failed to download ${asset} from ${PROXOR_QT_RUNTIME_REPO} release ${PROXOR_QT_RUNTIME_TAG}." >&2
  echo "Publish the Qt runtime archive in this repository, then rerun the script." >&2
  return 1
}

if [ "${DL_QT_VER:-}" == "5.15" ]; then
  download_asset "${PROXOR_QT5_ASSET:-Proxor-Qt5.15.7-Windows-x86_64-VS2019-16.11.20-2026-03-23.7z}"
else
  download_asset "${PROXOR_QT6_ASSET:-Proxor-Qt6.7.2-Windows-x86_64-VS2022-17.10.3-2026-03-23.7z}"
fi

7z x "$DOWNLOADED_ASSET"
rm -f "$DOWNLOADED_ASSET"
rm -rf Qt
mv Qt* Qt
