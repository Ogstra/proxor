#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"

grep -q 'DescribePlatformEnvironment(ProxorPlatform::CurrentPlatformEnvironment())' "$repo_root/src/ui/mainwindow.cpp" \
  || { echo "capabilities.sh: mainwindow.cpp must log the platform line" >&2; exit 1; }
grep -q 'CurrentPackageMode()' "$repo_root/src/platform/PlatformCapabilitiesApp.cpp" \
  || { echo "capabilities.sh: app glue must read CurrentPackageMode()" >&2; exit 1; }
grep -q 'platformName()' "$repo_root/src/platform/PlatformCapabilitiesApp.cpp" \
  || { echo "capabilities.sh: app glue must read platformName()" >&2; exit 1; }
if grep -nE 'QtWidgets|QtGui|QWidget|QGuiApplication|dataStore' \
    "$repo_root/src/platform/PlatformCapabilities.cpp" "$repo_root/src/platform/PlatformCapabilities.hpp"; then
  echo "capabilities.sh: the truth table must stay pure (no Widgets/Gui/dataStore)" >&2
  exit 1
fi
grep -q 'env\.compositor = ' "$repo_root/src/platform/PlatformCapabilitiesApp.cpp" \
  || { echo "capabilities.sh: app glue must pass the detected compositor to the platform line" >&2; exit 1; }
grep -q 'compositor=' "$repo_root/src/platform/PlatformCapabilities.cpp" \
  || { echo "capabilities.sh: the platform line must name the compositor" >&2; exit 1; }
