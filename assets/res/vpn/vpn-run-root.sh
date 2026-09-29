#!/bin/sh
set -e

CORE_PATH=${1:?missing core path}
CONFIG_PATH=${2:?missing config path}
TUN_NAME=${3:-}
SOCKS_PORT=${4:-}

if [ "$EUID" -ne 0 ]; then
  echo "[Warning] Tun script not running as root"
fi

command -v pkill >/dev/null 2>&1 || echo "[Warning] pkill not found"

BASEDIR=$(dirname "$0")
cd "$BASEDIR"

pre_start_linux() {
  # for Tun2Socket
  iptables -I INPUT -s 172.19.0.2 -d 172.19.0.1 -p tcp -j ACCEPT
  ip6tables -I INPUT -s fdfe:dcba:9876::2 -d fdfe:dcba:9876::1 -p tcp -j ACCEPT
}

wait_for_socks() {
  [ -z "$SOCKS_PORT" ] && return 0

  PORT_HEX=$(printf '%04X' "$SOCKS_PORT")
  attempt=0
  while [ "$attempt" -lt 100 ]; do
    if awk -v port=":$PORT_HEX" '$2 ~ port && $4 == "0A" { found = 1 } END { exit !found }' /proc/net/tcp /proc/net/tcp6 2>/dev/null; then
      return 0
    fi
    attempt=$((attempt + 1))
    sleep 0.1
  done

  echo "Timed out waiting for the local SOCKS listener on port $SOCKS_PORT" >&2
  return 1
}

start() {
  echo "PROXOR_TUN_AUTHORIZED"
  wait_for_socks || return $?
  pre_start_linux
  "$CORE_PATH" run -c "$CONFIG_PATH" &
  CORE_PID=$!

  if [ -z "$TUN_NAME" ] || ! command -v ip >/dev/null 2>&1; then
    echo "PROXOR_TUN_READY"
    wait "$CORE_PID"
    return $?
  fi

  attempt=0
  while [ "$attempt" -lt 100 ]; do
    if ip link show dev "$TUN_NAME" >/dev/null 2>&1; then
      echo "PROXOR_TUN_READY"
      wait "$CORE_PID"
      return $?
    fi
    if ! kill -0 "$CORE_PID" >/dev/null 2>&1; then
      wait "$CORE_PID"
      return $?
    fi
    attempt=$((attempt + 1))
    sleep 0.1
  done

  echo "Timed out waiting for TUN interface $TUN_NAME" >&2
  kill "$CORE_PID" 2>/dev/null || true
  wait "$CORE_PID" 2>/dev/null || true
  return 1
}

stop() {
  iptables -D INPUT -s 172.19.0.2 -d 172.19.0.1 -p tcp -j ACCEPT
  ip6tables -D INPUT -s fdfe:dcba:9876::2 -d fdfe:dcba:9876::1 -p tcp -j ACCEPT
}

if start; then
  STATUS=0
else
  STATUS=$?
fi
stop || true
exit "$STATUS"
