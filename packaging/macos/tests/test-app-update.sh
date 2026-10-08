#!/bin/bash
# Tests of packaging/macos/proxor-app-update.sh on THROWAWAY bundles only.
#
# Every bundle, zip, log and result file lives under this script's own mktemp -d and is removed on exit.
# The bundles have their own bundle id (io.github.Ogstra.Proxor.updtest), are named UpdTest*.app and are never Proxor:
# nothing here touches the real install, the helper, the config or the network. Throwaway processes are found by
# their path under the temp dir (never by name) and killed by pid. The relauncher always runs in a stock
# environment: env -i PATH=/usr/bin:/bin.
#
# usage: bash packaging/macos/tests/test-app-update.sh      (macOS only; prints "test-app-update.sh: OK")
# bash 3.2 compatible.
set -u

if [ "$(uname -s)" != Darwin ]; then
  echo "test-app-update.sh: SKIP (not macOS)"
  exit 0
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
REL="$HERE/../proxor-app-update.sh"
MK="$HERE/make-test-app.sh"
PB=/usr/libexec/PlistBuddy
LSREG=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister

FAILS=0
CHECKS=0
pass() { CHECKS=$((CHECKS + 1)); }
fail() { CHECKS=$((CHECKS + 1)); FAILS=$((FAILS + 1)); echo "FAIL: $*"; }
check() { # check "description" command args...
  local what=$1
  shift
  if "$@" >/dev/null 2>&1; then pass; else fail "$what"; fi
}

if [ ! -f "$REL" ]; then
  echo "FAIL: relauncher missing: $REL"
  exit 1
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/updtest-run.XXXXXX")" || exit 1
WORK="$(cd "$WORK" && pwd)"
WORK_P="$(cd "$WORK" && pwd -P)"
OPENED=""
OLDPIDS=""

# kill every process whose command starts with a path under the temp dir (both spellings), by pid
reap() {
  local pids
  pids="$(ps -axww -o pid= -o args= | WA="$WORK/" WB="$WORK_P/" /usr/bin/awk '
    { line = $0; sub(/^ +/, "", line); pid = line; sub(/ .*/, "", pid); args = substr(line, length(pid) + 2)
      if (substr(args, 1, length(ENVIRON["WA"])) == ENVIRON["WA"] || substr(args, 1, length(ENVIRON["WB"])) == ENVIRON["WB"]) print pid }')"
  [ -z "$pids" ] || kill $pids 2>/dev/null
  local p
  for p in $OLDPIDS; do
    if [ "$(ps -p "$p" -o args= 2>/dev/null | sed 's/^ *//')" = "sleep 30" ]; then kill "$p" 2>/dev/null; fi
  done
  return 0
}
cleanup() {
  local b
  reap
  sleep 0.3
  reap
  for b in $OPENED; do [ ! -x "$LSREG" ] || "$LSREG" -u "$b" >/dev/null 2>&1; done
  chmod -R u+w "$WORK" 2>/dev/null
  rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 1' INT TERM HUP

# ---------------------------------------------------------------- helpers
HOST_ARCH="$(uname -m)"
case $HOST_ARCH in
  arm64) OTHER_ARCH=x86_64 ;;
  *) OTHER_ARCH=arm64 ;;
esac

plist() { "$PB" -c "Print :$2" "$1/Contents/Info.plist" 2>/dev/null; }
version_of() { plist "$1" CFBundleShortVersionString; }
pkgzip() { # pkgzip <bundle> <zip>   (the flags of libs/package_macos.sh)
  rm -f "$2"
  ditto -c -k --keepParent --norsrc --noextattr --noqtn --noacl "$1" "$2"
}

CASE=""
D=""
TARGET=""
LOG=""
RESULT=""
ZIPF=""
OLD=""
newcase() { # newcase <name> [target app name]
  CASE=$1
  D="$WORK/$1"
  mkdir -p "$D/.proxor-update"
  TARGET="$D/${2:-UpdTest.app}"
  LOG="$D/$(basename "${2:-UpdTest.app}" .app)-run.log"
  RESULT="$D/result.txt"
  ZIPF="$D/.proxor-update/update.zip"
  : > "$D/args.none"
}
mk_old() { # mk_old <version> [options]   builds the app being updated at $TARGET
  local v=$1
  shift
  bash "$MK" "$D" "$(basename "$TARGET")" "$v" --result-file "$RESULT" "$@" >/dev/null 2>&1 || fail "$CASE: could not build the old app"
  [ "$(dirname "$TARGET")" = "$D" ] || mv "$D/$(basename "$TARGET")" "$TARGET"
}
mk_zip() { # mk_zip <version> [options]   builds the update at $D/new/UpdTest.app and zips it to $ZIPF
  local v=$1
  shift
  mkdir -p "$D/new"
  bash "$MK" "$D/new" UpdTest.app "$v" --result-file "$RESULT" --zip "$ZIPF" "$@" >/dev/null 2>&1 || fail "$CASE: could not build the update"
}
start_old() { # a fake "GUI" process, quit by the test after $1 seconds (default 1)
  sleep 30 &
  OLD=$!
  OLDPIDS="$OLDPIDS $OLD"
  if [ "${1:-1}" != never ]; then
    ( sleep "${1:-1}"; kill "$OLD" 2>/dev/null ) >/dev/null 2>&1 &
  fi
}
LAUNCH=exec
WAITS=20
ALIVES=3
OPENCMD=""
run_rel() { # run_rel <pid> <zip> <target> <result> [-- args...]  -> RC
  local seams="PROXOR_APP_UPDATE_WAIT_SECONDS=$WAITS PROXOR_APP_UPDATE_ALIVE_SECONDS=$ALIVES"
  [ -z "$LAUNCH" ] || seams="$seams PROXOR_APP_UPDATE_LAUNCH=$LAUNCH"
  [ -z "$OPENCMD" ] || seams="$seams PROXOR_APP_UPDATE_OPEN=$OPENCMD"
  # shellcheck disable=SC2086
  env -i PATH=/usr/bin:/bin HOME="$HOME" TMPDIR="${TMPDIR:-/tmp}" $seams /bin/bash "$REL" install "$@" > "$D/rel.out" 2>&1
  RC=$?
}
first_line() { head -1 "$1" 2>/dev/null; }
result_is() { # result_is <prefix>
  case "$(first_line "$RESULT")" in
    "$1"*) return 0 ;;
    *) return 1 ;;
  esac
}
leftovers() { # hidden stage/backup/zip-dir leftovers in $1
  ls -A "$1" 2>/dev/null | grep -E '^\.proxor-update-stage\.|^\..*\.previous\.|^\.proxor-update$' | head -1
}
no_leftovers() { [ -z "$(leftovers "${1:-$D}")" ]; }
log_has() { grep -qE "$1" "$LOG" 2>/dev/null; }
no_unbound() { ! grep -q 'unbound variable' "$D/rel.out"; }
signature_ok() { codesign --verify --deep --strict "$1"; }
quarantine_count() { xattr -lr "$1" 2>/dev/null | grep -c com.apple.quarantine; }
good_args() { printf '%s\n' "-many" "-appdata" "$D/appdata"; }
expect() { # expect "description" cmd args...  (cmd run in the current shell, success = pass)
  local what="$CASE: $1"
  shift
  if "$@" >/dev/null 2>&1; then pass; else fail "$what"; fi
}
reap_case() { reap; sleep 0.3; }
rcis() { [ "$RC" = "$1" ]; }

# ---------------------------------------------------------------- shim assertion
shim_check() {
  local f n body bad=""
  body="$(grep -v '^[[:space:]]*#' "$REL")"
  for n in lipo otool vtool xcrun clang python3; do
    if printf '%s\n' "$body" | grep -qw "$n"; then bad="$bad $n"; fi
  done
  for f in /usr/bin/*; do
    [ -f "$f" ] || continue
    if grep -qa libxcselect "$f" 2>/dev/null; then
      n="$(basename "$f")"
      # a command position: line start, after ; & | ( ` $( then do else if ! or a leading /usr/bin/
      if printf '%s\n' "$body" | grep -qE "(^|[;&|(\`]|\\\$\\(|[[:space:]](then|do|else|if|!))[[:space:]]*(/usr/bin/)?$n([[:space:]]|\$)"; then
        bad="$bad $n"
      fi
    fi
  done
  if [ -n "$bad" ]; then fail "the relauncher names Xcode command-line-tool shims:$bad"; else pass; fi
}
shim_check

# ---------------------------------------------------------------- 1 success, args pass through
newcase c01-success
mk_old 1.0.0
mk_zip 2.0.0
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" -- -many -appdata "$D/appdata"
reap_case
expect "exit 0" rcis 0
expect "result ok 2.0.0" test "$(first_line "$RESULT")" = "ok 2.0.0"
expect "target is 2.0.0" test "$(version_of "$TARGET")" = 2.0.0
expect "signature valid" signature_ok "$TARGET"
expect "new app got the arguments" grep -qF "start -many -appdata $D/appdata" "$LOG"
expect "13: the new app saw ok 2.0.0 when it started" grep -qx 'result=ok 2.0.0' "$LOG"
expect "no leftovers" no_leftovers
expect "no unbound variable" no_unbound
expect "zip removed" test ! -e "$ZIPF"

# ---------------------------------------------------------------- 1b Finder-style launches
newcase c01b-no-dashdash
mk_old 1.0.0
mk_zip 2.0.0
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 0" rcis 0
expect "ok" test "$(first_line "$RESULT")" = "ok 2.0.0"
expect "no extra arguments" grep -qE '^2\.0\.0 [0-9]+ start$' "$LOG"
expect "no unbound variable" no_unbound

newcase c01b-empty-dashdash
mk_old 1.0.0
mk_zip 2.0.0
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" --
reap_case
expect "exit 0" rcis 0
expect "ok" test "$(first_line "$RESULT")" = "ok 2.0.0"
expect "no extra arguments" grep -qE '^2\.0\.0 [0-9]+ start$' "$LOG"
expect "no unbound variable" no_unbound

# ---------------------------------------------------------------- 2 open launch (needs a GUI session)
if launchctl print "gui/$(id -u)" >/dev/null 2>&1; then
  newcase c02-open
  mk_old 1.0.0
  mk_zip 2.0.0
  OPENED="$OPENED $TARGET"
  start_old
  LAUNCH=""
  run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" -- -many -appdata "$D/appdata"
  LAUNCH=exec
  reap_case
  expect "exit 0" rcis 0
  expect "ok" test "$(first_line "$RESULT")" = "ok 2.0.0"
  expect "new app started with the arguments" grep -qF "start -many -appdata $D/appdata" "$LOG"
  expect "no leftovers" no_leftovers
  echo "case 2 (open): ran with a GUI session"
else
  echo "case 2 (open): SKIPPED, no GUI session"
fi

# ---------------------------------------------------------------- 2b exec fallback when open fails
newcase c02b-open-fails
mk_old 1.0.0
mk_zip 2.0.0
start_old
LAUNCH=""
OPENCMD=false
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" -- -many -appdata "$D/appdata"
LAUNCH=exec
OPENCMD=""
reap_case
expect "exit 0" rcis 0
expect "ok" test "$(first_line "$RESULT")" = "ok 2.0.0"
expect "exec fallback started 2.0.0" grep -qF "start -many -appdata $D/appdata" "$LOG"

# ---------------------------------------------------------------- 2c exact alive match
newcase c02c-helper-not-the-app
mk_old 1.0.0
mk_zip 2.0.0 --exit 1 --helper-exec
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 1" rcis 1
expect "failed start" result_is "failed start:"
expect "helper really ran" grep -qE '^2\.0\.0 [0-9]+ helper$' "$LOG"
expect "restored 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 2d lingering process from the old bundle
newcase c02d-lingering-helper
mk_old 1.0.0 --helper-exec
mk_zip 2.0.0
"$TARGET/Contents/MacOS/helper" >/dev/null 2>&1 &
sleep 0.5
start_old
WAITS=2
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
WAITS=20
reap_case
expect "exit 1" rcis 1
expect "failed wait, old process" test "$(first_line "$RESULT")" = "failed wait: an old Proxor process is still running"
expect "nothing swapped" test "$(version_of "$TARGET")" = 1.0.0
expect "untouched target relaunched" log_has '^1\.0\.0 [0-9]+ start'
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 3 wait timeout
newcase c03-wait-timeout
mk_old 1.0.0
mk_zip 2.0.0
start_old never
WAITS=2
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
WAITS=20
reap_case
expect "exit 1" rcis 1
expect "failed wait" result_is "failed wait:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "nothing staged" no_leftovers
expect "no second instance while the old one runs" test ! -s "$LOG"

# ---------------------------------------------------------------- 4 corrupt zip
newcase c04-corrupt-zip
mk_old 1.0.0
mk_zip 2.0.0
head -c 1200 "$ZIPF" > "$ZIPF.cut" && mv "$ZIPF.cut" "$ZIPF"
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 1" rcis 1
expect "failed extract" result_is "failed extract:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "v1 relaunched" log_has '^1\.0\.0 [0-9]+ start'
expect "13: result written before the old app restarted" sh -c "grep -A1 '^1\\.0\\.0 [0-9]* start' '$LOG' | grep -q '^result=failed extract'"
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 5 different bundle id
newcase c05-other-bundle-id
mk_old 1.0.0
mk_zip 2.0.0 --id io.github.Ogstra.Proxor.updtest.other
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 1" rcis 1
expect "failed verify" result_is "failed verify:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "v1 relaunched" log_has '^1\.0\.0 [0-9]+ start'
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 6 broken signature
newcase c06-broken-signature
mk_old 1.0.0
mkdir -p "$D/new"
bash "$MK" "$D/new" UpdTest.app 2.0.0 >/dev/null 2>&1 || fail "c06: could not build the update"
echo "tampered after signing" >> "$D/new/UpdTest.app/Contents/Resources/data.txt"
pkgzip "$D/new/UpdTest.app" "$ZIPF"
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 1" rcis 1
expect "failed verify" result_is "failed verify:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 7 no shared architecture
newcase c07-no-shared-arch
mk_old 1.0.0
mk_zip 2.0.0 --arch "$OTHER_ARCH"
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 1" rcis 1
expect "failed verify" result_is "failed verify:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 8 needs a newer macOS
newcase c08-min-macos
mk_old 1.0.0
mk_zip 2.0.0 --min 99.0
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 1" rcis 1
expect "failed verify" result_is "failed verify:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 9 new version does not stay running
newcase c09-new-exits
mk_old 1.0.0
mk_zip 2.0.0 --exit 1
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" -- -many -appdata "$D/appdata"
reap_case
expect "exit 1" rcis 1
expect "failed start" result_is "failed start:"
expect "restored 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "signature valid" signature_ok "$TARGET"
expect "v1 relaunched with the arguments" grep -qE "^1\\.0\\.0 [0-9]+ start -many -appdata $D/appdata" "$LOG"
expect "13: result written before the old app restarted" sh -c "grep -A1 '^1\\.0\\.0 [0-9]* start' '$LOG' | grep -q '^result=failed start'"
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 10 quarantined zip
newcase c10-quarantine
mk_old 1.0.0
mkdir -p "$D/new"
bash "$MK" "$D/new" UpdTest.app 2.0.0 --result-file "$RESULT" >/dev/null 2>&1 || fail "c10: could not build the update"
find "$D/new/UpdTest.app" -exec xattr -w com.apple.quarantine "0081;00000000;UpdTest;" {} +
rm -f "$ZIPF"
ditto -c -k --keepParent "$D/new/UpdTest.app" "$ZIPF"
xattr -w com.apple.quarantine "0081;00000000;UpdTest;" "$ZIPF"
mkdir "$D/probe"
ditto -x -k "$ZIPF" "$D/probe"
expect "precondition: the zip carries the quarantine flag into the extraction" test "$(quarantine_count "$D/probe/UpdTest.app")" -gt 0
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
expect "exit 0" rcis 0
expect "ok" test "$(first_line "$RESULT")" = "ok 2.0.0"
expect "no quarantine left" test "$(quarantine_count "$TARGET")" = 0
expect "signature valid" signature_ok "$TARGET"

# ---------------------------------------------------------------- 11 read-only parent
newcase c11-read-only-parent
mkdir "$D/ro" "$D/zips"
TARGET="$D/ro/UpdTest.app"
LOG="$D/ro/UpdTest-run.log"
ZIPF="$D/zips/update.zip"
mk_old 1.0.0
mk_zip 2.0.0
chmod a-w "$D/ro"
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT"
reap_case
chmod u+w "$D/ro"
expect "exit 1" rcis 1
expect "failed extract" result_is "failed extract:"
expect "target still 1.0.0" test "$(version_of "$TARGET")" = 1.0.0
expect "nothing created in the parent" no_leftovers "$D/ro"

# ---------------------------------------------------------------- 12 spaces and parentheses
newcase c12-spaces "UpdTest Prox (1).app"
mk_old 1.0.0
mk_zip 2.0.0
start_old
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" -- -many -appdata "$D/appdata"
reap_case
expect "exit 0" rcis 0
expect "ok" test "$(first_line "$RESULT")" = "ok 2.0.0"
expect "target is 2.0.0" test "$(version_of "$TARGET")" = 2.0.0
expect "new app started" grep -qF "start -many -appdata $D/appdata" "$LOG"
expect "no leftovers" no_leftovers

# ---------------------------------------------------------------- 13a validation (exit 2, nothing touched)
newcase c13a-validation
mk_old 1.0.0
mk_zip 2.0.0
start_old never
cp -R "$TARGET" "$D/Notanapp"
run_rel "$OLD" "$ZIPF" "$D/Notanapp" "$RESULT"
expect "target not ending .app: exit 2" rcis 2
run_rel "$OLD" "$ZIPF" "$D/Missing.app" "$RESULT"
expect "target missing: exit 2" rcis 2
run_rel notapid "$ZIPF" "$TARGET" "$RESULT"
expect "pid not a number: exit 2" rcis 2
run_rel "$OLD" "$D/nozip.zip" "$TARGET" "$RESULT"
expect "zip missing: exit 2" rcis 2
run_rel "$OLD" "$ZIPF" "$TARGET" "$RESULT" extra-without-dashdash
expect "arguments without --: exit 2" rcis 2
run_rel "$OLD" "$ZIPF" "relative/UpdTest.app" "$RESULT"
expect "relative target: exit 2" rcis 2
reap_case
expect "target untouched" test "$(version_of "$TARGET")" = 1.0.0
expect "zip untouched" test -s "$ZIPF"
expect "nothing staged" test -z "$(ls -A "$D" | grep -E '^\.proxor-update-stage\.|^\..*\.previous\.')"
expect "no app was started" test ! -s "$LOG"

# 14 (reveal) does not exist: FALLBACK=none, `reveal` must be a usage error
newcase c14-no-reveal
mk_old 1.0.0
mk_zip 2.0.0
run_rel_reveal() { env -i PATH=/usr/bin:/bin HOME="$HOME" /bin/bash "$REL" reveal "$ZIPF" "$D/dest" "$RESULT" > "$D/rel.out" 2>&1; RC=$?; }
run_rel_reveal
expect "reveal is not a subcommand: exit 2" rcis 2

reap
if [ "$FAILS" -ne 0 ]; then
  echo "test-app-update.sh: $FAILS of $CHECKS checks FAILED"
  exit 1
fi
echo "test-app-update.sh: $CHECKS checks passed"
echo "test-app-update.sh: OK"
