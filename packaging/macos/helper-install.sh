#!/bin/sh
# Installs the Proxor privileged helper (Tun and System Proxy service) as a launchd system daemon.
#
# Usage: helper-install.sh <uid> <src-binary> <sha256> <app-path>
#
# Run once as root, through the admin prompt (osascript ... with administrator privileges).
#
# Accepted trade-off (documented for users): this script itself is read from the user-writable
# app bundle when the admin prompt runs it, same as Clash Verge's service mode. What it installs
# is a root-owned, hash-verified copy of proxor_core, so the running daemon never executes
# anything user-writable.
set -eu
umask 022
PATH=/usr/bin:/bin:/usr/sbin:/sbin
export PATH

LABEL=io.github.Ogstra.Proxor.helper
BIN=/Library/PrivilegedHelperTools/$LABEL
PLIST=/Library/LaunchDaemons/$LABEL.plist
SUP="/Library/Application Support/Proxor"
SOCK=/var/run/$LABEL.sock

die() {
  code=$1
  shift
  echo "helper-install: $*" >&2
  exit "$code"
}

[ "$#" -eq 4 ] || die 2 "usage: helper-install.sh <uid> <src-binary> <sha256> <app-path>"
UID_ARG=$1
SRC=$2
WANT=$3
APP=$4

# ---- validate everything before touching the filesystem ----
case $UID_ARG in
  '' | *[!0-9]*) die 2 "uid must be a positive integer" ;;
  0*) die 2 "uid must not be 0" ;;
esac

case $WANT in
  *[!0-9a-f]*) die 2 "sha256 must be 64 lowercase hex characters" ;;
esac
[ "${#WANT}" -eq 64 ] || die 2 "sha256 must be 64 lowercase hex characters"

case $SRC in
  /*) ;;
  *) die 2 "source binary must be an absolute path" ;;
esac
[ -f "$SRC" ] || die 2 "source binary is not a regular file"
[ ! -L "$SRC" ] || die 2 "source binary must not be a symlink"

case $APP in
  /*) ;;
  *) die 2 "app path must be absolute" ;;
esac
case $APP in
  *.app) ;;
  *) die 2 "app path must end in .app" ;;
esac
case $APP in
  *'
'*) die 2 "app path must not contain a newline" ;;
esac
[ -d "$APP" ] || die 2 "app path is not a directory"

# ---- root is required only after validation, so the static test can exercise it unprivileged ----
[ "$(id -u)" = 0 ] || die 5 "must run as root"

for d in /Library/PrivilegedHelperTools /Library/LaunchDaemons "$SUP"; do
  [ ! -L "$d" ] || die 2 "$d is a symlink"
done
mkdir -p /Library/PrivilegedHelperTools /Library/LaunchDaemons "$SUP"
chown root:wheel "$SUP"
chmod 0755 "$SUP"

# Reinstall/update path: stop the running daemon first.
launchctl bootout "system/$LABEL" 2>/dev/null || true

# ---- root-owned, hash-verified copy of the helper binary ----
rm -f "$BIN.new"
cp "$SRC" "$BIN.new"
actual=$(shasum -a 256 "$BIN.new" | cut -d' ' -f1)
if [ "$actual" != "$WANT" ]; then
  rm -f "$BIN.new"
  die 3 "sha256 mismatch (expected $WANT, got $actual)"
fi
chown root:wheel "$BIN.new"
chmod 0755 "$BIN.new"
mv -f "$BIN.new" "$BIN"
xattr -c "$BIN" 2>/dev/null || true

# ---- uid allowlist (append, idempotent) and app path ----
UIDS="$SUP/allowed-uids"
rm -f "$UIDS.new"
if [ -f "$UIDS" ]; then
  cp "$UIDS" "$UIDS.new"
else
  : >"$UIDS.new"
fi
if ! grep -qx "$UID_ARG" "$UIDS.new"; then
  echo "$UID_ARG" >>"$UIDS.new"
fi
chown root:wheel "$UIDS.new"
chmod 0644 "$UIDS.new"
mv -f "$UIDS.new" "$UIDS"

rm -f "$SUP/app-path.new"
printf '%s\n' "$APP" >"$SUP/app-path.new"
chown root:wheel "$SUP/app-path.new"
chmod 0644 "$SUP/app-path.new"
mv -f "$SUP/app-path.new" "$SUP/app-path"

# ---- LaunchDaemon plist (root:wheel 0644 or launchd refuses it) ----
rm -f "$PLIST.new"
# BEGIN PLIST
cat >"$PLIST.new" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key>
  <string>io.github.Ogstra.Proxor.helper</string>
  <key>AssociatedBundleIdentifiers</key>
  <array>
    <string>io.github.Ogstra.Proxor</string>
  </array>
  <key>ProgramArguments</key>
  <array>
    <string>/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper</string>
    <string>helper</string>
  </array>
  <key>RunAtLoad</key>
  <true/>
  <key>KeepAlive</key>
  <dict>
    <key>SuccessfulExit</key>
    <false/>
  </dict>
  <key>ThrottleInterval</key>
  <integer>10</integer>
  <key>StandardOutPath</key>
  <string>/var/log/proxor-helper.log</string>
  <key>StandardErrorPath</key>
  <string>/var/log/proxor-helper.log</string>
</dict>
</plist>
PLIST
# END PLIST
chown root:wheel "$PLIST.new"
chmod 0644 "$PLIST.new"
plutil -lint "$PLIST.new" >/dev/null || { rm -f "$PLIST.new"; die 4 "generated plist is invalid"; }
mv -f "$PLIST.new" "$PLIST"

# ---- start ----
launchctl enable "system/$LABEL" 2>/dev/null || true
if ! out=$(launchctl bootstrap system "$PLIST" 2>&1); then
  echo "$out" >&2
  die 4 "launchctl bootstrap failed"
fi

i=0
while [ "$i" -lt 20 ]; do
  [ -S "$SOCK" ] && break
  sleep 0.5
  i=$((i + 1))
done
[ -S "$SOCK" ] || die 4 "the service did not start (check System Settings > General > Login Items & Extensions)"

echo "PROXOR_HELPER_INSTALLED"
