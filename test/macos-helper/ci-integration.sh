#!/usr/bin/env bash
# Root integration test for the Proxor macOS helper. Runs ONLY on the GitHub macos-15 runner (which
# has passwordless sudo), through:  sudo bash test/macos-helper/ci-integration.sh <path-to-Proxor.app>
#
# NEVER run this on a developer Mac: it installs a LaunchDaemon and changes the system proxy.
#
# Exit codes: 0 ok, 1 failure, 78 neutral SKIP (the runner refused `launchctl bootstrap`, which
# Background Task Management can do to an unapproved daemon).
set -euo pipefail

if [ "$(id -u)" != 0 ]; then
  echo "ci-integration.sh must run as root (sudo)" >&2
  exit 1
fi
APP="${1:?usage: ci-integration.sh <path-to-Proxor.app>}"
RUNNER_UID="${SUDO_UID:?run through sudo so SUDO_UID names the runner user}"
case "$APP" in /*) ;; *) echo "app path must be absolute" >&2; exit 1 ;; esac

HERE="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
LABEL=io.github.Ogstra.Proxor.helper
BIN=/Library/PrivilegedHelperTools/$LABEL
PLIST=/Library/LaunchDaemons/$LABEL.plist
SUP="/Library/Application Support/Proxor"
SOCK=/var/run/$LABEL.sock
INSTALL_SH="$APP/Contents/Resources/helper/helper-install.sh"
UNINSTALL_SH="$APP/Contents/Resources/helper/helper-uninstall.sh"
CORE="$APP/Contents/MacOS/proxor_core"
SOCKS_PORT=21080
PROXY_PORT=18080

PY="$(command -v python3)"
export PYTHONDONTWRITEBYTECODE=1
# World-readable copies, so the runner user and `nobody` can run the client. Under /tmp on purpose:
# root's $TMPDIR (/var/folders/...) is a 0700 directory nobody else can enter.
WORK="$(mktemp -d /tmp/proxor-ci.XXXXXX)"
chmod 755 "$WORK"
cp "$HERE/helper_client.py" "$WORK/helper_client.py"
chmod 644 "$WORK/helper_client.py"
CLIENT=("$PY" "$WORK/helper_client.py")

SOCKS_PID=""
HOLD_PID=""

cleanup() {
  local rc=$?
  set +e
  if [ "$rc" != 0 ] && [ "$rc" != 78 ]; then
    echo "---- diagnostics (exit $rc) ----" >&2
    launchctl print "system/$LABEL" 2>&1 | head -30 >&2
    tail -60 /var/log/proxor-helper.log >&2 2>&1
    scutil --proxy 2>&1 | head -40 >&2
    netstat -rn -f inet 2>&1 | grep -E '^198\.51\.100|utun' >&2
  fi
  [ -n "$HOLD_PID" ] && kill "$HOLD_PID" 2>/dev/null
  pkill -f "$WORK/helper_client.py" 2>/dev/null
  [ -n "$SOCKS_PID" ] && kill "$SOCKS_PID" 2>/dev/null
  sh "$UNINSTALL_SH" >/dev/null 2>&1
  rm -rf "$WORK"
  exit "$rc"
}
trap cleanup EXIT

step() { echo "== STEP $1: $2 OK"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

as_runner() { sudo -u "#$RUNNER_UID" "$@"; }

# route_present: any 198.51.100.x route (the TEST-NET-2 test route) in the IPv4 table.
# (Output is captured first: `cmd | grep -q` under pipefail can fail on SIGPIPE.)
route_present() {
  local t
  t="$(netstat -rn -f inet)"
  grep -qE '^198\.51\.100' <<<"$t"
}

wait_until() { # wait_until <seconds> <command...>
  local n=$1 i=0
  shift
  while [ "$i" -lt $((n * 4)) ]; do
    if "$@"; then return 0; fi
    sleep 0.25
    i=$((i + 1))
  done
  "$@"
}

helper_gone() {
  [ ! -e "$PLIST" ] && [ ! -e "$BIN" ] && [ ! -e "$SUP" ] && [ ! -e "$SOCK" ]
}

install_helper() { # sets INSTALL_RC and leaves the output in $WORK/install.out
  local sha
  sha="$(shasum -a 256 "$CORE" | cut -d' ' -f1)"
  INSTALL_RC=0
  sh "$INSTALL_SH" "$RUNNER_UID" "$CORE" "$sha" "$APP" >"$WORK/install.out" 2>&1 || INSTALL_RC=$?
  cat "$WORK/install.out"
}

# ---- STEP 1: install ---------------------------------------------------------------------------
[ -x "$CORE" ] || fail "no executable proxor_core at $CORE"
install_helper
if [ "$INSTALL_RC" = 4 ] && grep -Eq 'launchctl bootstrap|did not start' "$WORK/install.out"; then
  echo "SKIP root-daemon: $(tr '\n' ' ' <"$WORK/install.out")"
  launchctl print "system/$LABEL" 2>&1 | head -20 || true
  sh "$UNINSTALL_SH" || true
  exit 78
fi
[ "$INSTALL_RC" = 0 ] || fail "helper-install.sh exited $INSTALL_RC"
grep -q PROXOR_HELPER_INSTALLED "$WORK/install.out" || fail "install output lacks PROXOR_HELPER_INSTALLED"
step 1 "install"

# ---- STEP 2: files and service state -----------------------------------------------------------
[ "$(stat -f '%Su:%Sg %Lp' "$BIN")" = "root:wheel 755" ] || fail "binary is $(stat -f '%Su:%Sg %Lp' "$BIN")"
[ "$(stat -f '%Su:%Sg %Lp' "$PLIST")" = "root:wheel 644" ] || fail "plist is $(stat -f '%Su:%Sg %Lp' "$PLIST")"
plutil -lint "$PLIST" >/dev/null || fail "plist does not lint"
codesign --verify --strict "$BIN" || fail "installed binary fails codesign --verify --strict"
running() {
  local t
  t="$(launchctl print "system/$LABEL" 2>&1)" || true
  grep -q 'state = running' <<<"$t"
}
wait_until 10 running || fail "launchd does not report state = running"
[ -S "$SOCK" ] || fail "no socket at $SOCK"
step 2 "files, signature and launchd state"

# ---- STEP 3: authentication by peer uid --------------------------------------------------------
as_runner "${CLIENT[@]}" hello || fail "the runner user got no hello reply"
NOBODY_RC=0
sudo -u nobody "${CLIENT[@]}" hello || NOBODY_RC=$?
[ "$NOBODY_RC" = 3 ] || fail "uid nobody: expected exit 3 (no reply), got $NOBODY_RC"
step 3 "auth (allowed uid answered, nobody got no reply)"

# ---- STEP 4: Tun with a TEST-NET-2-only route --------------------------------------------------
if [ "${PROXOR_CI_SKIP_TUN:-}" = 1 ]; then
  echo "== STEP 4: tun SKIP (PROXOR_CI_SKIP_TUN=1: the hosted VM refuses to create a utun)"
else
  as_runner "$PY" -c "
import socket
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('127.0.0.1', $SOCKS_PORT))
s.listen(16)
while True:
    c, _ = s.accept()
    c.close()
" &
  SOCKS_PID=$!
  wait_until 10 bash -c "exec 3<>/dev/tcp/127.0.0.1/$SOCKS_PORT" 2>/dev/null || fail "mock SOCKS did not start"
  sed "s/%PORT%/$SOCKS_PORT/g" "$HERE/tun-test-config.json" >"$WORK/tun.json"
  chmod 644 "$WORK/tun.json"
  as_runner "${CLIENT[@]}" tun --config "$WORK/tun.json" --port "$SOCKS_PORT" || fail "tun scenario failed"
  gone() { ! route_present; }
  wait_until 5 gone || fail "198.51.100 route still present after the connection closed"
  kill "$SOCKS_PID" 2>/dev/null || true
  SOCKS_PID=""
  step 4 "tun (utun route up, tun_stop and connection close each removed it)"
fi

# ---- STEP 5: system proxy ----------------------------------------------------------------------
# Compared without `<Kind>Enable : 0` lines: a service that never had a proxy has no such key, and
# networksetup can only set it to 0 (never delete it), so absent and 0 are the same state.
norm_proxy() { scutil --proxy | grep -Ev '^[[:space:]]*[A-Za-z]*Enable[[:space:]]*:[[:space:]]*0[[:space:]]*$' || true; }
norm_proxy >"$WORK/before"
chmod 644 "$WORK/before"
proxy_shown() {
  local t
  t="$(scutil --proxy)"
  grep -Eq "HTTPProxy[[:space:]]*:[[:space:]]*127\.0\.0\.1" <<<"$t" &&
    grep -Eq "HTTPPort[[:space:]]*:[[:space:]]*$PROXY_PORT" <<<"$t" &&
    grep -Eq "SOCKSPort[[:space:]]*:[[:space:]]*$PROXY_PORT" <<<"$t"
}
proxy_restored() { [ "$(norm_proxy)" = "$(cat "$WORK/before")" ]; }
stop_holder() {
  pkill -f "$WORK/helper_client.py sysproxy-apply" 2>/dev/null || true
  if [ -n "$HOLD_PID" ]; then wait "$HOLD_PID" 2>/dev/null || true; fi
  HOLD_PID=""
}
# apply_hold: returns 0 applied, 4 no eligible service (skip signal); anything else fails the run.
apply_hold() {
  local i=0 rc
  : >"$WORK/apply.out"
  as_runner "${CLIENT[@]}" sysproxy-apply --port "$PROXY_PORT" --hold >"$WORK/apply.out" 2>&1 &
  HOLD_PID=$!
  while [ "$i" -lt 60 ]; do
    if grep -q '^APPLIED' "$WORK/apply.out"; then return 0; fi
    if ! kill -0 "$HOLD_PID" 2>/dev/null; then
      rc=0
      wait "$HOLD_PID" || rc=$?
      HOLD_PID=""
      cat "$WORK/apply.out" >&2
      if [ "$rc" = 4 ]; then return 4; fi
      fail "sysproxy-apply exited $rc"
    fi
    sleep 0.25
    i=$((i + 1))
  done
  cat "$WORK/apply.out" >&2
  fail "sysproxy-apply did not report APPLIED within 15 s"
}

APPLY_RC=0
apply_hold || APPLY_RC=$?
if [ "$APPLY_RC" = 4 ]; then
  echo "SKIP sysproxy: the runner has no network service with a hardware device"
  echo "== STEP 5: sysproxy SKIP (no eligible service)"
else
  wait_until 10 proxy_shown || { scutil --proxy >&2; fail "scutil --proxy does not show 127.0.0.1:$PROXY_PORT after apply"; }
  as_runner "${CLIENT[@]}" sysproxy-restore || fail "sysproxy-restore failed"
  wait_until 10 proxy_restored || { diff "$WORK/before" <(norm_proxy) >&2 || true; fail "scutil --proxy differs from the pre-test state after restore"; }
  stop_holder

  as_runner "${CLIENT[@]}" sysproxy-cycle --port "$PROXY_PORT" --before "$WORK/before" || fail "sysproxy-cycle failed"

  # lease: closing the connection restores the proxy
  apply_hold || fail "second apply failed"
  wait_until 10 proxy_shown || fail "proxy not shown before the lease test"
  stop_holder
  wait_until 10 proxy_restored || fail "connection close did not restore the system proxy"

  # crash recovery: kill -9 the helper while the proxy is applied; launchd restarts it and
  # RecoverAtStart restores the snapshot
  apply_hold || fail "third apply failed"
  wait_until 10 proxy_shown || fail "proxy not shown before the crash test"
  LAUNCHCTL_OUT="$(launchctl print "system/$LABEL")"
  HELPER_PID="$(awk '/^[[:space:]]*pid = / {print $3; exit}' <<<"$LAUNCHCTL_OUT")"
  [ -n "$HELPER_PID" ] || fail "cannot find the helper pid"
  kill -9 "$HELPER_PID"
  stop_holder
  wait_until 40 proxy_restored || { diff "$WORK/before" <(norm_proxy) >&2 || true; fail "system proxy not restored after the helper was killed"; }
  wait_until 10 bash -c "[ -S '$SOCK' ]" || fail "helper did not come back after kill -9"
  as_runner "${CLIENT[@]}" hello >/dev/null || fail "restarted helper does not answer"
  step 5 "system proxy (apply, restore, stop-start cycle, lease close, kill -9 recovery)"
fi

# ---- STEP 6: uninstall command -----------------------------------------------------------------
as_runner "${CLIENT[@]}" uninstall || fail "uninstall command failed"
wait_until 15 helper_gone || fail "files remain after the uninstall command: $(ls -d "$PLIST" "$BIN" "$SUP" "$SOCK" 2>/dev/null | tr '\n' ' ')"
if launchctl print "system/$LABEL" >/dev/null 2>&1; then fail "launchd still knows the service after uninstall"; fi
step 6 "uninstall command"

# ---- STEP 7: reinstall, then the uninstall script (twice: idempotent) --------------------------
install_helper
[ "$INSTALL_RC" = 0 ] || fail "reinstall exited $INSTALL_RC"
sh "$UNINSTALL_SH" || fail "helper-uninstall.sh failed"
helper_gone || fail "files remain after helper-uninstall.sh"
sh "$UNINSTALL_SH" || fail "helper-uninstall.sh is not idempotent"
step 7 "reinstall, helper-uninstall.sh, idempotent rerun"

echo "ci-integration.sh: OK"
