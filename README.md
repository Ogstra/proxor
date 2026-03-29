# Proxor

Qt-based proxy client for managing sing-box profiles, subscriptions, routing, and system proxy integration.

Current version: `1.4.0`

<img width="804" height="634" alt="image" src="https://github.com/user-attachments/assets/67d27033-e809-4b89-a5dc-2dcd800163de" />

## Supported Proxy Types

- SOCKS (4/4a/5)
- HTTP(S)
- Shadowsocks
- VMess
- VLESS
- Trojan
- TUIC
- NaiveProxy
- Hysteria2
- Custom outbound
- Custom config
- Custom core

## Documentation

- [Build Windows](docs/Build_Windows.md)
- [Build Linux](docs/Build_Linux.md)
- [Build Core](docs/Build_Core.md)
- [Linux Runtime Guide](docs/Run_Linux.md)
- [Run Flags](docs/RunFlags.md)

## Repository Layout

- `src/` contains the Qt/C++ product source tree.
- `assets/` contains Qt resources and release-owned assets.
- `go/` remains the dedicated Go core and updater workspace root.
- `3rdparty/` and `libs/` remain the dedicated vendored-code and dependency helper roots.
- `scripts/` and `cmake/` remain the dedicated build and tooling roots.
- `build/` remains the default GUI build root.
- `.tmp/` and `.cache/` are the default generated-output roots for local deployment bundles and transient artifacts; `qtsdk/` remains the local toolchain root.
- `deployment/` remains a legacy local output location still accepted when passed explicitly to the scripts.

## Runtime Notes

### Windows

If the application reports missing runtime DLLs, install the [Microsoft Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).

### Linux

Linux packaging and deployment vary by distribution. Use the local build and runtime guides in [`docs/`](docs/readme.md) instead of old third-party package references.
Linux remains manual-build only for now. `.deb`, AppImage, and CI-produced Linux release artifacts are not currently supported.

## Dependencies

### GUI and Native Build

- Qt 5.15 or Qt 6 Widgets/Network/Svg
- protobuf C++ (`v21.4` in `libs/build_deps_all.sh`)
- yaml-cpp (`0.7.0`)
- zxing-cpp (`2.0.0`)
- QHotkey

### Core and Go Toolchain

- Go `1.26.x`
- sing-box from the local git submodule (`3rdparty/sing-box`, base `v1.13.3`, fork `Ogstra/sing-box`)
- proxorlib from the local workspace
- gRPC `v1.79.3`
- protobuf-go `v1.36.11`

## Credits

- Original desktop project lineage: [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray)
- Current backend foundation: `sing-box`, `sing`, and `proxorlib`
- UI/editor components adapted from [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- Native libraries: Qt, protobuf, yaml-cpp, zxing-cpp, QHotkey
