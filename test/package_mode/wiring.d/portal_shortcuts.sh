#!/usr/bin/env bash
set -euo pipefail
repo_root="$1"
fail() { echo "portal_shortcuts.sh: $1" >&2; exit 1; }
gs="$repo_root/src/sys/linux/PortalGlobalShortcuts.cpp"
tr="$repo_root/src/platform/PortalShortcutTrigger.cpp"
for needle in BindShortcuts CreateSession '"Activated"' org.freedesktop.portal.Session; do
  grep -qF -- "$needle" "$gs" || fail "PortalGlobalShortcuts.cpp must contain $needle"
done
if grep -qF '// Stub from 52-04' "$gs"; then fail "PortalGlobalShortcuts.cpp is still the 52-04 stub"; fi
if grep -qE '#include <(QtGui|QKeySequence|QGui)' "$tr" || grep -qE 'include <Qt?Gui' "$tr"; then fail "PortalShortcutTrigger.cpp must not include QtGui"; fi
if grep -n 'Q_OS_' "$gs" "$tr" "$repo_root/src/sys/linux/PortalGlobalShortcuts.hpp" "$repo_root/src/platform/PortalShortcutTrigger.hpp" >&2; then fail "no Q_OS_ in these sources"; fi
if grep -qE 'QtWidgets|QWidget|QtGui' "$gs"; then fail "PortalGlobalShortcuts.cpp must stay QtCore + QtDBus"; fi
echo "portal_shortcuts: OK"
