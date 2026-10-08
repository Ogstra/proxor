#!/usr/bin/env bash
# Drift guard between the Go helper (server) and the C++ client: protocol version, every command/
# event name and JSON key, the three fixed paths and the default System Proxy bypass list must be
# equal on both sides. Runs on Linux and macOS with POSIX tools only.
#
#   test-protocol-version.sh              self-test, then the real check
#   test-protocol-version.sh --self-test  only prove the checks are not vacuous
set -euo pipefail

root="$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)"

GO_PROTO=go/cmd/proxor_core/machelper/protocol.go
GO_PLAN=go/cmd/proxor_core/machelper/sysproxy_plan.go
CPP_POLICY_H=src/sys/macos/MacHelperPolicy.h
CPP_POLICY_CPP=src/sys/macos/MacHelperPolicy.cpp
CPP_CLIENT_H=src/sys/macos/MacHelperClient.h
INSTALL_SH=packaging/macos/helper-install.sh
CASK_IN=packaging/homebrew/proxor.rb.in

FILES=("$GO_PROTO" "$GO_PLAN" "$CPP_POLICY_H" "$CPP_POLICY_CPP" "$CPP_CLIENT_H" "$INSTALL_SH" "$CASK_IN")

# ---- extractors (all take the tree root as $1) -------------------------------------------------

go_protocol() {
  sed -nE 's/^const ProtocolVersion = ([0-9]+).*/\1/p' "$1/$GO_PROTO" | head -1
}

cpp_protocol() {
  sed -nE 's/.*kMacHelperProtocolVersion = ([0-9]+);.*/\1/p' "$1/$CPP_POLICY_H" | head -1
}

go_wire_names() {
  {
    grep -E '^[[:space:]]+(Cmd|Event)[A-Za-z]+[[:space:]]+= "[^"]*"' "$1/$GO_PROTO" |
      sed -E 's/.*= "([^"]*)".*/\1/'
    grep -oE 'json:"[^"]*"' "$1/$GO_PROTO" | sed -E 's/json:"([^",]*).*/\1/'
  } | sort -u
}

cpp_wire_names() {
  awk '/^namespace MacHelperWire \{/ {on=1; next} on && /^\}/ {on=0} on {print}' "$1/$CPP_CLIENT_H" |
    grep -E 'constexpr const char \*k(Cmd|Event|Key)[A-Za-z]+ = "[^"]*"' |
    sed -E 's/.*= "([^"]*)".*/\1/' | sort -u
}

go_path() { # $1 root, $2 Go constant name
  sed -nE "s/^const $2 = \"([^\"]*)\".*/\1/p" "$1/$GO_PROTO" | head -1
}

cpp_path() { # $1 root, $2 C++ constant name
  awk '/^namespace MacHelperWire \{/ {on=1; next} on && /^\}/ {on=0} on {print}' "$1/$CPP_CLIENT_H" |
    sed -nE "s/.*\\*$2 = \"([^\"]*)\".*/\1/p" | head -1
}

go_bypass() {
  awk '/^func DefaultBypass\(\)/ {on=1; next} on && /^\}/ {on=0} on {print}' "$1/$GO_PLAN" |
    grep -oE '"[^"]*"'
}

cpp_bypass() {
  awk '/^QStringList MacDefaultProxyBypass\(\)/ {on=1; next} on && /^\}/ {on=0} on {print}' "$1/$CPP_POLICY_CPP" |
    grep -oE '"[^"]*"'
}

# The install script builds its paths from $LABEL; expand it so the check is on the final string.
install_text() {
  local label
  label="$(sed -nE 's/^LABEL=(.*)$/\1/p' "$1/$INSTALL_SH" | head -1)"
  [ -n "$label" ] || return 1
  sed "s/\\\$LABEL/$label/g" "$1/$INSTALL_SH"
}

# ---- the check (tree root as $1); prints every difference, returns 1 on any ---------------------

run_check() {
  local r="$1" bad=0 n=0 gp cp
  local tmp
  tmp="$(mktemp -d)"

  gp="$(go_protocol "$r")"
  cp="$(cpp_protocol "$r")"
  if [ -z "$gp" ] || [ -z "$cp" ]; then
    echo "FAIL: protocol version not found (go='$gp' cpp='$cp')" >&2
    bad=1
  elif [ "$gp" != "$cp" ]; then
    echo "FAIL: protocol version differs: go $gp, cpp $cp" >&2
    bad=1
  fi

  go_wire_names "$r" >"$tmp/go.names"
  cpp_wire_names "$r" >"$tmp/cpp.names"
  n="$(wc -l <"$tmp/go.names" | tr -d ' ')"
  if [ "$n" -lt 10 ]; then
    echo "FAIL: only $n Go wire names found; the extraction is broken" >&2
    bad=1
  fi
  if ! diff "$tmp/go.names" "$tmp/cpp.names" >"$tmp/names.diff"; then
    echo "FAIL: wire names differ (< Go, > C++):" >&2
    cat "$tmp/names.diff" >&2
    bad=1
  fi

  install_text "$r" >"$tmp/install.txt" || { echo "FAIL: LABEL not found in $INSTALL_SH" >&2; bad=1; }
  local pair goname cppname gv cv
  for pair in SocketPath:kSocketPath LaunchDaemonPlistPath:kPlistPath HelperBinaryPath:kBinaryPath; do
    goname="${pair%%:*}"
    cppname="${pair##*:}"
    gv="$(go_path "$r" "$goname")"
    cv="$(cpp_path "$r" "$cppname")"
    if [ -z "$gv" ] || [ -z "$cv" ]; then
      echo "FAIL: path $goname/$cppname not found (go='$gv' cpp='$cv')" >&2
      bad=1
      continue
    fi
    if [ "$gv" != "$cv" ]; then
      echo "FAIL: $goname differs: go '$gv', cpp '$cv'" >&2
      bad=1
    fi
    if ! grep -qF -- "$gv" "$tmp/install.txt"; then
      echo "FAIL: $gv does not appear in $INSTALL_SH" >&2
      bad=1
    fi
    if [ "$goname" != SocketPath ] && ! grep -qF -- "$gv" "$r/$CASK_IN"; then
      echo "FAIL: $gv does not appear in $CASK_IN" >&2
      bad=1
    fi
  done

  go_bypass "$r" >"$tmp/go.bypass"
  cpp_bypass "$r" >"$tmp/cpp.bypass"
  local m
  m="$(wc -l <"$tmp/go.bypass" | tr -d ' ')"
  if [ "$m" -lt 5 ]; then
    echo "FAIL: only $m Go bypass entries found; the extraction is broken" >&2
    bad=1
  fi
  if ! diff "$tmp/go.bypass" "$tmp/cpp.bypass" >"$tmp/bypass.diff"; then
    echo "FAIL: default bypass list differs (< Go, > C++):" >&2
    cat "$tmp/bypass.diff" >&2
    bad=1
  fi

  CHECK_NAMES="$n"
  CHECK_BYPASS="$m"
  CHECK_PROTOCOL="$gp"
  rm -rf "$tmp"
  return "$bad"
}

# ---- self-test: every mutation must make the check fail -----------------------------------------

copy_tree() {
  local dst="$1" f
  for f in "${FILES[@]}"; do
    mkdir -p "$dst/$(dirname "$f")"
    cp "$root/$f" "$dst/$f"
  done
}

expect_fail() { # $1 label, $2 file, $3 sed expression
  local label="$1" file="$2" expr="$3" t
  t="$(mktemp -d)"
  copy_tree "$t"
  if ! run_check "$t" >/dev/null 2>&1; then
    rm -rf "$t"
    echo "FAIL: self-test baseline copy does not pass" >&2
    exit 1
  fi
  sed -E "$expr" "$t/$file" >"$t/mutated"
  if cmp -s "$t/mutated" "$t/$file"; then
    rm -rf "$t"
    echo "FAIL: self-test mutation '$label' changed nothing (pattern is stale)" >&2
    exit 1
  fi
  mv "$t/mutated" "$t/$file"
  if run_check "$t" >/dev/null 2>&1; then
    rm -rf "$t"
    echo "FAIL: self-test: mutation '$label' was NOT detected" >&2
    exit 1
  fi
  rm -rf "$t"
}

self_test() {
  expect_fail "renamed C++ key" "$CPP_CLIENT_H" 's/"socksPort"/"socks_port"/'
  expect_fail "renamed Go json tag" "$GO_PROTO" 's/json:"socksPort,omitempty"/json:"socks_port,omitempty"/'
  expect_fail "renamed C++ command" "$CPP_CLIENT_H" 's/"sysproxy_apply"/"sysproxy_set"/'
  expect_fail "protocol version" "$CPP_POLICY_H" 's/kMacHelperProtocolVersion = 1;/kMacHelperProtocolVersion = 2;/'
  expect_fail "changed C++ bypass entry" "$CPP_POLICY_CPP" 's/QStringLiteral\("100\.64\.0\.0\/10"\)/QStringLiteral("100.64.0.0\/11")/'
  expect_fail "changed Go bypass entry" "$GO_PLAN" 's/"192\.168\.0\.0\/16"/"192.168.0.0\/15"/'
  expect_fail "changed socket path" "$CPP_CLIENT_H" 's#/var/run/io\.github\.Ogstra\.Proxor\.helper\.sock#/var/run/io.github.Ogstra.Proxor.helper2.sock#'
  expect_fail "cask binary path" "$CASK_IN" 's#/Library/PrivilegedHelperTools/io\.github\.Ogstra\.Proxor\.helper#/Library/PrivilegedHelperTools/other#'
}

self_test
if [ "${1:-}" = "--self-test" ]; then
  echo "test-protocol-version.sh: self-test OK"
  exit 0
fi

CHECK_NAMES=0 CHECK_BYPASS=0 CHECK_PROTOCOL=0
if ! run_check "$root"; then
  exit 1
fi
echo "test-protocol-version.sh: OK (protocol $CHECK_PROTOCOL, $CHECK_NAMES wire names, bypass $CHECK_BYPASS entries)"
