#!/bin/bash
# Replaces a manually installed Proxor.app with a newer one after the GUI has quit, and starts it again.
#
# usage: proxor-app-update.sh install <pid> <zip> <target.app> <result-file> [-- <app args>...]
#   <pid>          the running Proxor GUI; the script waits for it (and for anything still running from the
#                  target bundle) to exit. It never kills anything.
#   <zip>          the verified update zip (proxor-<version>-macos-<arch>.zip)
#   <target.app>   the installed bundle, replaced in place
#   <result-file>  one line, written atomically (<result-file>.tmp, then mv) and always BEFORE a relaunch:
#                    ok <CFBundleShortVersionString of the new bundle>
#                    failed <stage>: <message>        stage: wait | extract | verify | swap | start
#   after "--"     the arguments the new (or restored) app is started with; possibly none (a Finder launch has
#                  no arguments), so they are only ever handled as "$@", never as a bash array (bash 3.2, set -u).
# exit: 0 ok, 1 failed (the result file says why), 2 usage or validation error (nothing touched).
#
# The GUI copies this script to a temporary file before running it, because the bundle it ships in is replaced.
#
# Order: validate; wait for the old process; sweep stale siblings of this app; extract the zip into a hidden stage
# folder beside the target (same volume, so renames are atomic); verify the new bundle (exactly one app, same bundle
# id, valid ad-hoc signature, a shared CPU architecture, LSMinimumSystemVersion not above this macOS, the target
# owned by the current user); clear the quarantine flag when present; swap with a hidden backup; start the new app;
# keep the backup until the new process has stayed alive; otherwise restore the backup and start it again.
#
# Safety: nothing outside the given paths is touched; no sudo or admin prompt; no Xcode command-line-tool shim
# (CPU architectures come from /usr/bin/file -b, and PATH is /usr/bin:/bin); no process is ever killed.
# Failure before the swap leaves the target unchanged and starts it again; failure after the swap restores the backup.
#
# Two-rename window: between "mv target backup" and "mv staged target" there is briefly (about 2 ms measured) no
# app at the target path. If the script dies exactly there, the previous app survives as the hidden sibling
# ".<name>.previous.*" next to it and a single mv restores it. The GUI cannot help on its next start because it is
# the missing app, so the documentation says so.
#
# Test-only environment seams (the GUI never sets them):
#   PROXOR_APP_UPDATE_LAUNCH=exec        start the app by direct exec instead of /usr/bin/open
#   PROXOR_APP_UPDATE_WAIT_SECONDS=<n>   wait for the old process, default 60
#   PROXOR_APP_UPDATE_ALIVE_SECONDS=<n>  the new process must still run after n seconds, default 10
#   PROXOR_APP_UPDATE_OPEN=<command>     replaces /usr/bin/open (tests make it fail to force the exec fallback)
set -u
umask 022
PATH=/usr/bin:/bin
export PATH
trap '' HUP

PLB=/usr/libexec/PlistBuddy
WAIT_SECONDS=${PROXOR_APP_UPDATE_WAIT_SECONDS:-60}
ALIVE_SECONDS=${PROXOR_APP_UPDATE_ALIVE_SECONDS:-10}
LAUNCH_MODE=${PROXOR_APP_UPDATE_LAUNCH:-open}
OPEN_CMD=${PROXOR_APP_UPDATE_OPEN:-/usr/bin/open}
APPEAR_SECONDS=5

USAGE="usage: proxor-app-update.sh install <pid> <zip> <target.app> <result-file> [-- <app args>...]"
case ${WAIT_SECONDS}${ALIVE_SECONDS} in
  '' | *[!0-9]*) echo "$USAGE" >&2; echo "seam values must be whole numbers" >&2; exit 2 ;;
esac

[ "${1:-}" = install ] || { echo "$USAGE" >&2; exit 2; }
shift
[ $# -ge 4 ] || { echo "$USAGE" >&2; exit 2; }
PID=$1
ZIP=$2
TARGET=$3
RESULT=$4
shift 4
if [ $# -gt 0 ]; then
  [ "$1" = "--" ] || { echo "$USAGE" >&2; exit 2; }
  shift
fi
# "$@" now holds the app arguments (possibly none) and stays that way for the whole run.

STAGE=""
BACKUP=""

# ---- result file (atomic; written before any relaunch)
RESULT_OK=0
case $RESULT in
  /*) [ -d "$(dirname "$RESULT")" ] && RESULT_OK=1 ;;
esac
res() {
  [ "$RESULT_OK" = 1 ] || return 0
  printf '%s\n' "$1" > "$RESULT.tmp" 2>/dev/null && mv -f "$RESULT.tmp" "$RESULT" 2>/dev/null
  return 0
}
invalid() { # validation error: nothing touched
  res "failed verify: $1"
  echo "proxor-app-update: $1" >&2
  exit 2
}

# ---- validate everything before touching the filesystem
[ "$RESULT_OK" = 1 ] || { echo "proxor-app-update: result file must be an absolute path in an existing directory" >&2; exit 2; }
case $PID in
  '' | *[!0-9]*) invalid "the process id is not a number" ;;
esac
[ -f "$ZIP" ] || invalid "the update zip does not exist"
case $TARGET in
  /*) ;;
  *) invalid "the app path is not absolute" ;;
esac
TARGET=${TARGET%/}
case $TARGET in
  *.app) ;;
  *) invalid "the app path does not end in .app" ;;
esac
[ -L "$TARGET" ] && invalid "the app path is a symbolic link"
[ -d "$TARGET" ] || invalid "the app does not exist"
PARENT=$(dirname "$TARGET")
NAME=$(basename "$TARGET")
[ -d "$PARENT" ] || invalid "the app folder does not exist"
# LaunchServices reports the physical path (/tmp is /private/tmp), so the process checks know both spellings.
TARGET_P="$(cd -P "$PARENT" 2>/dev/null && pwd -P)/$NAME"

plget() { "$PLB" -c "Print :$2" "$1/Contents/Info.plist" 2>/dev/null; }
EXENAME=$(plget "$TARGET" CFBundleExecutable)
TARGET_ID=$(plget "$TARGET" CFBundleIdentifier)
[ -n "$EXENAME" ] && [ -n "$TARGET_ID" ] || invalid "the app has no readable Info.plist"
[ -f "$TARGET/Contents/MacOS/$EXENAME" ] || invalid "the app has no main executable"

# ---- process helpers (fixed-string tests in awk: the path may contain parentheses and spaces)
# pids whose command is exactly one of the two executable spellings, or starts with it followed by a space
exe_pids() {
  ps -axww -o pid= -o args= | E1="$1" E2="${2:-$1}" awk '
    { line = $0; sub(/^ +/, "", line); pid = line; sub(/ .*/, "", pid); args = substr(line, length(pid) + 2)
      for (k = 1; k <= 2; k++) { exe = ENVIRON["E" k]; n = length(exe)
        if (n > 0 && substr(args, 1, n) == exe && (length(args) == n || substr(args, n + 1, 1) == " ")) { print pid; break } } }'
}
# pids running anything under a directory (two spellings, each ending in /)
dir_pids() {
  ps -axww -o pid= -o args= | E1="$1" E2="${2:-$1}" awk '
    { line = $0; sub(/^ +/, "", line); pid = line; sub(/ .*/, "", pid); args = substr(line, length(pid) + 2)
      for (k = 1; k <= 2; k++) { d = ENVIRON["E" k]; n = length(d)
        if (n > 0 && substr(args, 1, n) == d) { print pid; break } } }'
}
# a <= b for dotted versions, missing parts count as 0
ver_le() {
  local pa pb first
  pa=$(printf '%s\n' "$1" | awk -F. '{ printf "%d.%d.%d", $1, $2, $3 }')
  pb=$(printf '%s\n' "$2" | awk -F. '{ printf "%d.%d.%d", $1, $2, $3 }')
  first=$(printf '%s\n%s\n' "$pa" "$pb" | sort -t. -k1,1n -k2,2n -k3,3n | head -1)
  [ "$first" = "$pa" ]
}
# CPU architectures of a Mach-O file, one per line
archs_of() { /usr/bin/file -b "$1" 2>/dev/null | grep -oE 'x86_64|arm64' | sort -u; }

# ---- start the app at $TARGET with the app arguments
launch() { # launch <executable name> app-args...
  local exe="$TARGET/Contents/MacOS/$1" exe_p="$TARGET_P/Contents/MacOS/$1" i
  shift
  if [ "$LAUNCH_MODE" != exec ]; then
    "$OPEN_CMD" -n "$TARGET" --args "$@" >/dev/null 2>&1
    i=0
    while [ "$i" -lt $((APPEAR_SECONDS * 5)) ]; do
      [ -n "$(exe_pids "$exe" "$exe_p")" ] && return 0
      sleep 0.2
      i=$((i + 1))
    done
  fi
  # direct exec: open failed, no process appeared within the time, or the test seam
  ( nohup "$exe" "$@" >/dev/null 2>&1 </dev/null & )
  return 0
}

drop_zip() {
  rm -f "$ZIP"
  case $(basename "$(dirname "$ZIP")") in
    .proxor-update) rmdir "$(dirname "$ZIP")" 2>/dev/null ;;
  esac
  return 0
}
drop_stage() { [ -z "$STAGE" ] || rm -rf "$STAGE"; STAGE=""; }

# failure before the swap: result first, then give the user the untouched app back
fail_before_swap() { # <stage> <message> app-args...
  local st=$1 msg=$2
  shift 2
  res "failed $st: $msg"
  drop_stage
  drop_zip
  launch "$EXENAME" "$@"
  exit 1
}

# ---- 1. wait for the old process, then for anything still running from the bundle
i=0
while kill -0 "$PID" 2>/dev/null; do
  i=$((i + 1))
  if [ "$i" -gt $((WAIT_SECONDS * 5)) ]; then
    # the old app is still running: do not start a second instance, nothing was touched
    res "failed wait: Proxor did not quit within ${WAIT_SECONDS} seconds"
    drop_zip
    exit 1
  fi
  sleep 0.2
done
while [ -n "$(dir_pids "$TARGET/Contents/MacOS/" "$TARGET_P/Contents/MacOS/")" ]; do
  i=$((i + 1))
  if [ "$i" -gt $((WAIT_SECONDS * 5)) ]; then
    res "failed wait: an old Proxor process is still running"
    drop_zip
    launch "$EXENAME" "$@"
    exit 1
  fi
  sleep 0.2
done

# ---- 2. sweep stale siblings of this app (an interrupted earlier run); the target exists, so they are not needed
for stale in "$PARENT"/.proxor-update-stage.* "$PARENT"/."$NAME".previous.*; do
  [ -d "$stale" ] && rm -rf "$stale"
done

# ---- 3. extract into a stage folder beside the target
STAGE=$(mktemp -d "$PARENT/.proxor-update-stage.XXXXXX" 2>/dev/null) || { STAGE=""; fail_before_swap extract "cannot create a staging folder in $PARENT" "$@"; }
ditto -x -k "$ZIP" "$STAGE" >/dev/null 2>&1 || fail_before_swap extract "could not extract the update" "$@"

# ---- 4. verify the new bundle
n=0
NEW=""
for a in "$STAGE"/*.app; do
  if [ -d "$a" ] && [ ! -L "$a" ]; then
    n=$((n + 1))
    NEW=$a
  fi
done
[ "$n" = 1 ] || fail_before_swap verify "the update does not contain exactly one app" "$@"
NEW_EXE=$(plget "$NEW" CFBundleExecutable)
NEW_ID=$(plget "$NEW" CFBundleIdentifier)
NEW_VER=$(plget "$NEW" CFBundleShortVersionString)
NEW_MIN=$(plget "$NEW" LSMinimumSystemVersion)
[ "$NEW_ID" = "$TARGET_ID" ] || fail_before_swap verify "the update has bundle id ${NEW_ID:-none}, expected $TARGET_ID" "$@"
[ -n "$NEW_EXE" ] && [ -f "$NEW/Contents/MacOS/$NEW_EXE" ] || fail_before_swap verify "the update has no main executable" "$@"
[ -n "$NEW_VER" ] || fail_before_swap verify "the update has no version" "$@"
codesign --verify --deep --strict "$NEW" >/dev/null 2>&1 || fail_before_swap verify "the update's code signature is not valid" "$@"
common=""
for x in $(archs_of "$NEW/Contents/MacOS/$NEW_EXE"); do
  for y in $(archs_of "$TARGET/Contents/MacOS/$EXENAME"); do
    [ "$x" = "$y" ] && common=1
  done
done
[ -n "$common" ] || fail_before_swap verify "the update has no CPU architecture in common with this app" "$@"
if [ -n "$NEW_MIN" ]; then
  ver_le "$NEW_MIN" "$(sw_vers -productVersion)" || fail_before_swap verify "the update needs macOS $NEW_MIN or later" "$@"
fi
[ "$(stat -f %u "$TARGET")" = "$(id -u)" ] || fail_before_swap verify "the installed app is not owned by the current user" "$@"

# ---- 5. quarantine (a flag set by a browser would make Gatekeeper block the first launch)
if xattr -lr "$NEW" 2>/dev/null | grep -q com.apple.quarantine; then
  xattr -dr com.apple.quarantine "$NEW" >/dev/null 2>&1
  if xattr -lr "$NEW" 2>/dev/null | grep -q com.apple.quarantine; then
    fail_before_swap verify "could not clear the download quarantine of the update" "$@"
  fi
fi

# ---- 6. swap with a hidden backup
BACKUP="$PARENT/.$NAME.previous.$RANDOM$RANDOM"
while [ -e "$BACKUP" ]; do BACKUP="$PARENT/.$NAME.previous.$RANDOM$RANDOM"; done
if ! mv "$TARGET" "$BACKUP" 2>/dev/null; then
  BACKUP=""
  fail_before_swap swap "could not move the current app aside" "$@"
fi
if ! mv "$NEW" "$TARGET" 2>/dev/null; then
  mv "$BACKUP" "$TARGET" 2>/dev/null
  BACKUP=""
  fail_before_swap swap "could not move the new app into place" "$@"
fi

# ---- 7. start the new version; the result line is written first
res "ok $NEW_VER"
launch "$NEW_EXE" "$@"
sleep "$ALIVE_SECONDS"
if [ -z "$(exe_pids "$TARGET/Contents/MacOS/$NEW_EXE" "$TARGET_P/Contents/MacOS/$NEW_EXE")" ]; then
  if mv "$TARGET" "$STAGE/failed.app" 2>/dev/null && mv "$BACKUP" "$TARGET" 2>/dev/null; then
    BACKUP=""
    res "failed start: the new version did not keep running; the previous version was restored"
    drop_stage
    drop_zip
    launch "$EXENAME" "$@"
  else
    res "failed start: the new version did not keep running and the previous version could not be restored; it is the hidden folder $BACKUP"
  fi
  exit 1
fi

# ---- 8. success: the backup is no longer needed
[ -z "$BACKUP" ] || rm -rf "$BACKUP"
drop_stage
drop_zip
exit 0
