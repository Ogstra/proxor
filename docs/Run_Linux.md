# Linux Runtime Guide

This document covers locally built packages and the x86_64 AppImage produced by GitHub Actions.

## Scope

Supported x86_64 channels are the retained AppImage, Debian/Ubuntu `.deb`, Fedora RPM, source
AUR `proxor`. `.deb` upgrades use apt/dpkg, RPM upgrades use dnf, and AUR upgrades use the
selected AUR helper.

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
asset to install and AUR names the selected AUR helper. None of them overwrite themselves in place, since their files belong to a
package manager that expects to be the one to replace them.

## Native packages

Native `.deb`, RPM, and AUR installations use the system Qt/runtime dependencies and retain the
interactive PolicyKit TUN authorization flow; no package install hook grants capabilities.
Do not use an in-app updater for Linux package-managed installs; update through the owning package manager.

## Wayland compositors

Hyprland, sway, niri, river, Wayfire and labwc run without a full desktop shell, so some desktop
services Proxor relies on must be added or configured in the compositor.

- **Tray:** the tray icon is a StatusNotifier item. It only shows if a bar hosts StatusNotifier
  items, for example Waybar's `tray` module. Without a tray, Proxor keeps its window open
  (minimized) instead of hiding it.
- **Start with system:** it writes an XDG autostart entry in `~/.config/autostart`, which
  Hyprland and sway do not run by themselves. Adding `exec-once = proxor` to `hyprland.conf` or
  `exec proxor` to the sway config may be required. A session tool that runs XDG autostart
  entries, such as uwsm, or `dex -a` started from the compositor config can also work. Check your
  compositor's documentation. For the AppImage use its path instead of `proxor`. Other compositors have their own startup command.
- **Global hotkeys:** they use the desktop's GlobalShortcuts portal. On Hyprland,
  xdg-desktop-portal-hyprland offers it, and a bind in `hyprland.conf` with the `global`
  dispatcher may also be required (`hyprctl globalshortcuts` may list what Proxor registered);
  check the Hyprland documentation. xdg-desktop-portal-wlr, used by sway and other wlroots
  compositors, has no GlobalShortcuts portal, so the hotkey fields in Settings are disabled there.
- **QR from the screen:** it uses the Screenshot portal, which xdg-desktop-portal-hyprland and
  xdg-desktop-portal-wlr provide. Adding a QR code from an image file or the clipboard always works.

In Settings, an option that is not available is disabled; hover it to see the reason in a tooltip.
System Proxy is set only on GNOME-family desktops and KDE Plasma, so on a compositor configure
apps manually or use Tun. The log file's `Platform:` line names the compositor, for example
`desktop=other compositor=hyprland`, which helps when reporting a problem.

## Wi-Fi network detection (On-Demand)

On-Demand reads the connected Wi-Fi network name from NetworkManager first (D-Bus, then `nmcli`).
When NetworkManager does not manage a Wi-Fi adapter, it reads the name from iwd instead.
Wi-Fi run only by wpa_supplicant or ConnMan is not supported.

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
