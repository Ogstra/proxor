# Installing Proxor on Linux by hand

Use this guide when you do not use a package repository: the AUR package is not published yet, and
`.deb`, `.rpm` and AppImage files come straight from a GitHub release. Only x86_64 is built.

## Pick a release

Open the [releases page](https://github.com/Ogstra/proxor/releases). The stable release may carry
only the Windows files; the Linux files are in the latest **prerelease**. Download the file for
your system from that release, together with `SHA256SUMS`, and check it before installing:

```bash
sha256sum -c --ignore-missing SHA256SUMS
```

## AppImage (any distribution)

```bash
chmod +x proxor-<version>-linux64.AppImage
./proxor-<version>-linux64.AppImage
```

- If it asks for FUSE (`libfuse.so.2` missing), install `libfuse2` (Debian/Ubuntu) or `fuse-libs`
  (Fedora), or run it without FUSE: `./proxor-<version>-linux64.AppImage --appimage-extract-and-run`.
- Keep the file somewhere you can write to, for example `~/Applications`: Proxor updates the
  AppImage in place and needs to replace that file.
- The AppImage stores its settings in your user data folder.

## Debian and Ubuntu (`.deb`)

```bash
sudo apt install ./proxor_<version>-1_amd64.deb
```

Remove it with `sudo apt remove proxor`. To update, install the newer `.deb` the same way.

## Fedora (`.rpm`)

```bash
sudo dnf install ./proxor-<version>-1.fc44.x86_64.rpm
```

The RPM is built on Fedora 44. On another Fedora version, or on openSUSE, it may ask for a Qt
version that is not available; use the AppImage instead. Remove it with `sudo dnf remove proxor`.

## Arch, CachyOS and other Arch-based systems (without the AUR)

The package recipe is the same one the AUR would use, built straight from a GitHub release:

```bash
git clone --depth 1 https://github.com/Ogstra/proxor
cd proxor
./packaging/arch/build-from-release.sh v<version>
```

This downloads the release's source archive, checks it against the release's `SHA256SUMS`, builds
the package with `makepkg` and installs it with `pacman`. Add `--render-only` to only write the
`PKGBUILD` (in `proxor-<version>-arch/`) and build it yourself with `makepkg -si`. It needs
`base-devel`, `cmake`, `ninja` and `go`; `makepkg --syncdeps` installs the runtime dependencies.
Remove it with `sudo pacman -R proxor`.

## From source

For any other distribution, see [Build Linux](Build_Linux.md).

## Tun mode and permissions

Proxor needs administrator rights only for Tun mode. It asks for them through PolicyKit
(`pkexec`), so install a PolicyKit agent for your desktop, and `iptables`/`ip6tables` if your
distribution does not include them. System Proxy is set on GNOME and KDE Plasma; on other
desktops configure your apps by hand or use Tun. See the [Linux runtime guide](Run_Linux.md) for
the details, including tray, autostart and hotkeys on Wayland compositors.

## Updating

- **AppImage:** Proxor offers the update itself and replaces the file.
- **`.deb`, `.rpm`, Arch package:** install the newer file or rebuild with the newer tag; Proxor
  tells you the command instead of replacing its own files.
