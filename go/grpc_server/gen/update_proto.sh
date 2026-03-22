#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if command -v protoc.exe >/dev/null 2>&1; then
  PROTOC_BIN="protoc.exe"
elif command -v protoc >/dev/null 2>&1; then
  PROTOC_BIN="protoc"
else
  echo "protoc is required but was not found in PATH" >&2
  exit 1
fi

if command -v go.exe >/dev/null 2>&1; then
  GO_BIN="go.exe"
elif command -v go >/dev/null 2>&1; then
  GO_BIN="go"
else
  echo "go is required but was not found in PATH" >&2
  exit 1
fi

"$GO_BIN" install google.golang.org/protobuf/cmd/protoc-gen-go@v1.36.11
"$GO_BIN" install google.golang.org/grpc/cmd/protoc-gen-go-grpc@v1.6.1

GOPATH_DIR="$("$GO_BIN" env GOPATH)"
if command -v wslpath >/dev/null 2>&1 && [[ "$GOPATH_DIR" =~ ^[A-Za-z]:\\ ]]; then
  GOPATH_DIR="$(wslpath "$GOPATH_DIR")"
fi
export PATH="$PATH:$GOPATH_DIR/bin"

"$PROTOC_BIN" -I . \
  --go_out=. \
  --go_opt=paths=source_relative \
  --go-grpc_out=. \
  --go-grpc_opt=paths=source_relative \
  libcore.proto
