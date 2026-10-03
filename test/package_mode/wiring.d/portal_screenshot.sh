#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "portal_screenshot.sh: $1" >&2; exit 1; }
f="$repo_root/src/sys/linux/PortalScreenshot.cpp"
h="$repo_root/src/sys/linux/PortalScreenshot.hpp"
for needle in '"interactive"' '"modal"' '/doc/' 'org.freedesktop.portal.Screenshot'; do
  grep -qF -- "$needle" "$f" || grep -qF -- "${needle//\"/}" "$f" || fail "PortalScreenshot.cpp must contain $needle"
done
grep -qF 'doc/' "$f" || fail "PortalScreenshot.cpp must exclude document-portal paths"
if grep -qF 'Stub from 52-04' "$f"; then fail "stub marker still present"; fi
if grep -n 'Q_OS_' "$f" "$h" >&2; then fail "no Q_OS_ in PortalScreenshot"; fi
if grep -qE 'ProxorGui|include "ui/|QtWidgets|QWidget' "$f" "$h"; then fail "PortalScreenshot must not include GUI code"; fi
echo "portal_screenshot: OK"
