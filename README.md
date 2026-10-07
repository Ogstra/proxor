# Proxor

[![Latest release](https://img.shields.io/github/v/release/Ogstra/proxor?include_prereleases&label=release)](https://github.com/Ogstra/proxor/releases)
[![License](https://img.shields.io/github/license/Ogstra/proxor)](LICENSE)

Qt client for sing-box: profiles, subscriptions, routing, Tun mode and system proxy.

<img width="717" height="559" alt="image" src="https://github.com/user-attachments/assets/d9b86402-7301-4e4e-971b-fef3ff2db247" />

## Install

Download from the [releases page](https://github.com/Ogstra/proxor/releases). The stable release
may only carry Windows files; macOS and Linux files are in the latest **prerelease**.

### Windows

1. Download `proxor-<version>-windows64.zip` and extract it.
2. Run `proxor.exe`.

If it reports missing DLLs, install the
[Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).

### macOS (Apple Silicon)

```bash
brew install --cask ogstra/tap/proxor
```

Update:

```bash
brew upgrade --cask proxor
```

Tun and System Proxy ask for your administrator password once, to install a small background
service. Details: [Build macOS](docs/Build_macOS.md#tun-and-system-proxy).

### Linux (x86_64)

AppImage:

```bash
chmod +x proxor-<version>-linux64.AppImage
./proxor-<version>-linux64.AppImage
```

Debian and Ubuntu:

```bash
sudo apt install ./proxor_<version>-1_amd64.deb
```

Fedora:

```bash
sudo dnf install ./proxor-<version>-1.fc44.x86_64.rpm
```

Arch and CachyOS, without the AUR:

```bash
git clone --depth 1 https://github.com/Ogstra/proxor
cd proxor
./packaging/arch/build-from-release.sh v<version>
```

More options, checksums and Tun permissions: [Install on Linux](docs/Install_Linux.md).

## Requirements

| System | Requirement |
|---|---|
| Windows | Windows 10 (1809 or later) or Windows 11, 64-bit |
| macOS | macOS 15 or later, Apple Silicon |
| Linux | x86_64 |

## Supported proxy types

SOCKS (4/4a/5), HTTP(S), Shadowsocks, VMess, VLESS, Trojan, TUIC, NaiveProxy, Hysteria2,
custom outbound, custom config, custom core.

## Documentation

- [Install on Linux](docs/Install_Linux.md)
- [Linux runtime guide](docs/Run_Linux.md)
- [Build Windows](docs/Build_Windows.md)
- [Build macOS](docs/Build_macOS.md)
- [Build Linux](docs/Build_Linux.md)
- [Build Core](docs/Build_Core.md)
- [Run flags](docs/RunFlags.md)

## License

GPL-3.0. See [LICENSE](LICENSE).

## Credits

- Original desktop project: [MatsuriDayo/nekoray](https://github.com/MatsuriDayo/nekoray)
- Backend: [sing-box](https://github.com/SagerNet/sing-box), `sing` and `proxorlib`
- UI components adapted from [Qv2ray](https://github.com/Qv2ray/Qv2ray)
- Libraries: Qt, protobuf, yaml-cpp, zxing-cpp, QHotkey
