# Build `proxor_core`

## Expected Workspace Layout

The repository expects the desktop project and the forked Go workspaces to live side by side:

```text
proxor/
  go/cmd/*
sing-box/
sing-box-extra/
```

The helper bootstrap script prepares that layout for local development.

## Bootstrap Sources

```bash
bash libs/get_source.sh
```

This script prepares the local fork/workspace layout used by the Go modules and `go.work`.

## Standard Build

Windows AMD64 example:

```bash
GOOS=windows GOARCH=amd64 bash libs/build_go.sh
```

The build script writes the resulting binaries into the matching deployment directory:

- `deployment/windows64`
- `deployment/windows-arm64`
- `deployment/linux64`
- `deployment/linux-arm64`

## Current Core Build Tags

The current repository build uses these sing-box tags:

```text
with_gvisor
with_quic
with_dhcp
with_wireguard
with_utls
with_acme
with_clash_api
with_tailscale
with_conntrack
with_grpc
```

These tags are defined in `libs/build_go.sh`.

## Notes

- `proxor_core` and `updater` are built from the Go workspace.
- The updater is renamed to `launcher` on Linux by `libs/build_go.sh`.
- If you are doing a fork-specific build, keep the local `replace` directives and `go.work` entries aligned with your sibling repositories.
