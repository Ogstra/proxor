# Build `proxor_core`

## Expected Workspace Layout

sing-box is a **git submodule** at `3rdparty/sing-box`, pointing at the fork declared in
`.gitmodules`. `go.work` wires it in directly:

```text
replace github.com/sagernet/sing-box => ./3rdparty/sing-box
```

There is no sibling-directory layout. An older `proxor/` + `sing-box/` side-by-side scheme
is no longer used, and `libs/get_source.sh` — which clones a sibling copy from upstream at a
pinned commit — is a leftover of it. That script is not part of the build: it clones the
wrong repository, and nothing reads what it produces. Do not run it.

## Bootstrap Sources

```bash
git submodule update --init --recursive
```

That is the whole bootstrap. If you cloned without `--recursive`, this is the step you
missed, and `go build` will fail with unresolved sing-box packages until you run it.

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
