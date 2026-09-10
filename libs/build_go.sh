#!/bin/bash
set -eu

source libs/env_deploy.sh

case "${GOOS:-}/${GOARCH:-}" in
  windows/amd64) DEST="$DEPLOYMENT/windows64" ;;
  windows/arm64) DEST="$DEPLOYMENT/windows-arm64" ;;
  linux/amd64) DEST="$DEPLOYMENT/linux64" ;;
  linux/arm64) DEST="$DEPLOYMENT/linux-arm64" ;;
  *)
    echo "Unsupported target: ${GOOS:-unset}/${GOARCH:-unset}" >&2
    exit 1
    ;;
esac

rm -rf "$DEST"
mkdir -p "$DEST"

export CGO_ENABLED=0

PROXOR_CORE_TAGS="with_gvisor,with_quic,with_dhcp,with_wireguard,with_utls,with_acme,with_clash_api,with_tailscale,with_conntrack,with_grpc"
# The Windows deployment includes Cronet's DLL, so enable the protocol exposed by
# the UI on that platform. Linux packaging needs its own Cronet runtime path.
if [ "$GOOS" = "windows" ] && [ "$GOARCH" = "amd64" ]; then
  PROXOR_CORE_TAGS="$PROXOR_CORE_TAGS,with_naive_outbound,with_purego"
fi

#### Go: updater ####
pushd go/cmd/updater
[ "$GOOS" = "darwin" ] || go build -o "$DEST" -trimpath -ldflags "-w -s"
[ "$GOOS" = "linux" ] && mv "$DEST/updater" "$DEST/launcher" || true
popd

#### Go: proxor_core ####
pushd go/cmd/proxor_core
go build -v -o "$DEST" -trimpath -ldflags "-w -s -X github.com/Ogstra/proxorlib/proxor_common.Version_proxor=$version_standalone" -tags "$PROXOR_CORE_TAGS"
if [ "$GOOS" = "windows" ] && [ "$GOARCH" = "amd64" ]; then
  cronet_dir="$(go list -m -f '{{.Dir}}' github.com/sagernet/cronet-go/lib/windows_amd64)"
  cp "$cronet_dir/libcronet.dll" "$DEST/"
fi
popd
