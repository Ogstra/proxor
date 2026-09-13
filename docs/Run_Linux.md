# Linux Runtime Guide

This document covers locally built packages and the x86_64 AppImage produced by GitHub Actions.

## Scope

Supported x86_64 channels are the retained AppImage, Debian/Ubuntu `.deb`, Fedora RPM, source
AUR `proxor`, and the GitHub Flatpak bundle. `.deb` upgrades use apt/dpkg, RPM upgrades use dnf,
AUR upgrades use the selected AUR helper, and Flatpak upgrades use `flatpak update`.

## AppImage

The AppImage stores configuration under the user application-data directory because its
mount is read-only. It includes geodata and Qt plugins. Tun mode is supported through a
separate privileged compatibility core, which requires PolicyKit (`pkexec`), `iptables`, and
`ip6tables`. The embedded core itself cannot receive `setcap` on the AppImage mount.

System Proxy integration is supported on GNOME-family desktops and KDE Plasma. Other desktop
environments must configure their proxy manually or use Tun mode.

## Update ownership per channel

The AppImage owns its own file, so it is the one Linux channel that updates itself in place:
it downloads the new `.AppImage` beside the running one, verifies it against the release's
`SHA256SUMS`, and replaces the file `$APPIMAGE` points at with an atomic same-directory
rename before relaunching into it. If that file or its directory is not writable, the
download is kept and the app tells you exactly where, and stays open rather than leaving you
with a closed app and no running version.

Every native package names its own update command instead: `.deb` and RPM name the release
asset to install, AUR names the selected AUR helper, and the GitHub Flatpak bundle names
`flatpak update`. None of them overwrite themselves in place, since their files belong to a
package manager that expects to be the one to replace them.

## Native packages and Flatpak

Native `.deb`, RPM, and AUR installations use the system Qt/runtime dependencies and retain the
interactive PolicyKit TUN authorization flow; no package install hook grants capabilities.
The Flatpak bundle intentionally has network/display permissions only. It cannot use TUN or
modify the host system proxy, so use it as a restricted profile client. Do not use an in-app
updater for Linux package-managed installs; update through the owning package manager.

## Runtime Options

After assembling a Linux package, there are two common launch paths:

1. `./proxor`
   Uses the system Qt runtime.

2. `./launcher`
   Uses the bundled runtime path prepared by the deployment flow.

## `launcher` Notes

- `./launcher -- -appdata`
  Passes `-appdata` through to the main application.

- `./launcher -debug`
  Starts the launcher in debug mode.

Some environments may require extra XCB-related runtime packages. For example, on Ubuntu 22.04 one common requirement is:

```bash
sudo apt install libxcb-xinerama0
```

## `proxor` Notes

Use `./proxor` when your system already provides a compatible Qt runtime. If the system runtime is incomplete or incompatible, prefer the bundled launch path instead.
