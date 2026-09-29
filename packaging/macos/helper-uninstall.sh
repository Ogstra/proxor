#!/bin/sh
# Removes the Proxor privileged helper (Tun and System Proxy service). Idempotent.
#
# Usage: helper-uninstall.sh   (no arguments; run as root through the admin prompt)
set -u
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

LABEL=io.github.Ogstra.Proxor.helper
BIN=/Library/PrivilegedHelperTools/$LABEL
PLIST=/Library/LaunchDaemons/$LABEL.plist
SUP="/Library/Application Support/Proxor"
SOCK=/var/run/$LABEL.sock

if [ "$(id -u)" != 0 ]; then
  echo "helper-uninstall: must run as root" >&2
  exit 5
fi

# The daemon's SIGTERM handler restores the system proxy and closes Tun.
launchctl bootout "system/$LABEL" 2>/dev/null || true

i=0
while [ "$i" -lt 10 ]; do
  [ -e "$SOCK" ] || break
  sleep 0.5
  i=$((i + 1))
done

rm -f "$PLIST" "$BIN" "$BIN.new" "$SOCK"
rm -rf "$SUP"

echo "PROXOR_HELPER_REMOVED"
exit 0
