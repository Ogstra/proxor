#!/bin/sh
# Runs a command on a private D-Bus session bus (dbus-run-session cannot start one on macOS).
# Usage: sh test/package_mode/run-on-private-bus.sh <binary> [args...]
conf=$(mktemp "${TMPDIR:-/tmp}/proxor-bus-conf.XXXXXX") || exit 1
out=$(mktemp "${TMPDIR:-/tmp}/proxor-bus-out.XXXXXX") || exit 1
pid=
cleanup() {
  [ -n "$pid" ] && kill "$pid" 2>/dev/null
  rm -f "$conf" "$out"
}
trap cleanup EXIT INT TERM
cat > "$conf" <<'XML'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><listen>unix:tmpdir=/tmp</listen>
<policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy></busconfig>
XML
addr=$(dbus-daemon --config-file="$conf" --fork --print-address=1 --print-pid=3 3>"$out") || exit 1
pid=$(cat "$out")
DBUS_SESSION_BUS_ADDRESS=$addr
export DBUS_SESSION_BUS_ADDRESS
"$@"
rc=$?
exit "$rc"
