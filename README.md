# NekoBox For PC

Qt-based cross-platform proxy manager for Windows and Linux.

This fork ships the desktop GUI as `nekobox` / `nekobox.exe` and the backend core as `nekobox_core` on top of the current sing-box integration used by this repository.

Current version: `4.0.4-2026-03-22`

<img width="801" height="631" alt="nekobox" src="https://github.com/user-attachments/assets/ca508369-cb98-485b-b8e4-dc942b423d9c" />

## Highlights

- Windows and Linux desktop builds
- sing-box based core workflow
- Profile management, subscriptions, routing, and traffic stats
- TUN mode and system proxy integration
- gRPC-based GUI/core communication

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

## Runtime Notes

### Windows

If the application reports missing runtime DLLs, install the [Microsoft Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).

### Linux

Linux packaging and deployment vary by distribution. Use the local build and runtime guides in [`docs/`](docs/readme.md) instead of old third-party package references.

## Dependencies

### GUI and Native Build

- Qt 5.15 or Qt 6 Widgets/Network/Svg
- protobuf C++ (`v21.4` in `libs/build_deps_all.sh`)
- yaml-cpp (`0.7.0`)
- zxing-cpp (`2.0.0`)
- QHotkey

### Core and Go Toolchain

- Go `1.26.x`
- sing-box fork from the local workspace
- libneko from the local workspace
- gRPC `v1.79.3`
- protobuf-go `v1.36.11`

## Credits

- Original desktop project lineage: [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray)
- Current backend foundation: `sing-box`, `sing`, and `libneko`
- UI/editor components adapted from [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- Native libraries: Qt, protobuf, yaml-cpp, zxing-cpp, QHotkey
