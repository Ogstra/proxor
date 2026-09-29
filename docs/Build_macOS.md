# Build Proxor on macOS

This document covers local Apple Silicon builds of the GUI application.

## Status

Releases carry `proxor-<version>-macos-arm64.zip`, built by the `package-macos` job of
`.github/workflows/build-proxor-cmake.yml` on the GitHub `macos-15` Apple Silicon runner. Apple
Silicon only, macOS 15 (Sequoia) or later. The app is ad-hoc signed and not notarized, so the zip
is not meant to be opened by double-click after a browser download (Gatekeeper blocks it): install
through Homebrew instead. `./libs/build_macos.sh` and `./libs/package_macos.sh` reproduce the same
build locally. macOS releases are published as prereleases.

## Install with Homebrew

```bash
brew install --cask ogstra/tap/proxor
brew upgrade --cask proxor
brew uninstall --cask proxor          # add --zap to also remove ~/Library/Preferences/proxor
```

Needs Homebrew 7 or newer (`brew update` first). The cask lives in `Ogstra/homebrew-tap` and is
bumped automatically when a release is published; it tracks prereleases too. The cask clears
`com.apple.quarantine` after install because the app is not notarized. A zip downloaded manually
needs `xattr -dr com.apple.quarantine Proxor.app`, or right-click and choose Open.

## Tun and System Proxy

Both switches (toolbar check boxes and App > Proxy Mode) work on macOS through a small Proxor
service. macOS releases are prereleases, so expect rough edges and report them.

- **How it works.** The first time you turn on Tun Mode or System Proxy, Proxor explains what it
  will do and macOS asks for an administrator password once. This installs the Proxor service:
  `/Library/LaunchDaemons/io.github.Ogstra.Proxor.helper.plist`,
  `/Library/PrivilegedHelperTools/io.github.Ogstra.Proxor.helper`, and a root-owned copy of
  `proxor_core`. After that Tun and System Proxy switch on and off without a password, including
  the remembered Tun at startup.
- **Background Items.** macOS shows a "Background Items Added" notification. Keep Proxor allowed in
  System Settings > General > Login Items & Extensions; if it is turned off, Tun settings shows
  "Installed but not running".
- **Security.** The service accepts connections only from the user(s) who installed it (kernel peer
  credentials), only runs a validated Tun configuration, never runs files from the app bundle, and
  restores your previous proxy settings when Proxor quits, crashes, or the Mac restarts.
- **Trade-off (same as Clash Verge's service mode).** When the administrator prompt runs, the
  one-time install script is read from the app bundle, which your user account can write to. What
  it installs is a root-owned copy of `proxor_core` checked against its SHA-256, so the running
  service never executes anything user-writable, but only approve the prompt for a Proxor.app you
  installed yourself (Homebrew or the release zip).
- **System Proxy follows the connection.** Stopping the profile (Stop, or the toolbar button) puts
  your previous proxy settings back while System Proxy stays on, and starting a profile points the
  Mac at Proxor again. Restart Proxy and switching profiles do not touch the settings. Quitting
  Proxor restores them.
- **Tun at launch.** If Tun cannot start at launch while it is remembered, Proxor does not connect
  until Tun works or you turn Tun Mode off ("Tun could not start: ... Turn off Tun Mode to connect
  without it."). If the service is missing (for example after an upgrade), Proxor connects without
  Tun and says so; it never asks for a password at launch.
- **Homebrew.** `brew uninstall --cask proxor` removes the service, and so does
  `brew upgrade --cask proxor` (Homebrew runs the old version's uninstall steps on upgrade). Both
  ask for your administrator password in the terminal, even if you never used Tun; this is an
  accepted trade-off. After an upgrade, turning on Tun Mode or System Proxy asks for the
  administrator password once more to reinstall the service. `--zap` also removes
  `/var/log/proxor-helper.log` and your preferences.
- **Removing manually.** Tun settings > Tun service > Remove (Tun Mode and System Proxy are turned
  off first), or `sudo launchctl bootout system/io.github.Ogstra.Proxor.helper` and then delete the
  three paths above. If the app is dragged to the Trash, the service removes itself after about 35
  minutes.
- **Limits.** `strict_route` has no effect on macOS; IPv6 traffic bypasses Tun unless "Enable IPv6"
  is on in Tun settings; macOS per-interface DNS can bypass the Tun DNS hijack; external cores
  (for example hysteria) are not excluded automatically; on Macs with several users each user
  installs once, and System Proxy is machine-wide.

## Prerequisites

- Xcode Command Line Tools (`xcode-select --install`)
- Homebrew
- Go (see the version pinned in `go/cmd/proxor_core/go.mod`, currently 1.26.1)
- CMake
- Ninja
- `brew install qtbase qtsvg protobuf yaml-cpp zxing-cpp`

Do not install the monolithic `qt` formula; `qtbase` and `qtsvg` are enough, and `qtbase`
ships `macdeployqt`.

## Clone the Repository

```bash
git clone https://github.com/Ogstra/proxor.git --recursive
cd proxor
```

## Build

```bash
./libs/build_macos.sh
```

This builds the darwin/arm64 `proxor_core`, configures and builds the Qt GUI, assembles
the bundle (`proxor_core` sits next to the GUI executable; the geodata files live in
`Contents/Resources/` with symlinks in `Contents/MacOS/`, so the signature survives zip/unzip),
runs `macdeployqt`, and finishes with an ad-hoc `codesign`. Output:
`deployment/macos-arm64/Proxor.app`.

Env knobs:

| Variable | Default | Meaning |
|---|---|---|
| `SKIP_GO` | unset | Set to `1` to reuse an already-built `proxor_core` and skip rebuilding it |
| `BUILD_DIR` | `build-macos` | CMake build tree used for the GUI |
| `MACDEPLOYQT` | auto-detected | Override the `macdeployqt` binary to use |
| `MACOSX_DEPLOYMENT_TARGET` | unset (SDK default) | Minimum macOS for the GUI build and `LSMinimumSystemVersion`; CI uses `15.0` |

Re-run the script after `brew upgrade qtbase protobuf` (and `rm -rf build-macos` first if
protobuf changed, since a stale build tree can mix headers from two protobuf versions).

## Package

```bash
./libs/package_macos.sh
```

Writes `deployment/proxor-<version>-macos-arm64.zip` with
`ditto --norsrc --noextattr --noqtn --noacl`, then re-extracts it with `/usr/bin/unzip` (as
Homebrew does) and runs `codesign --verify --deep --strict` on the result. It fails if any
bundled Mach-O needs a newer macOS than `MACOS_FLOOR` (default: `MACOSX_DEPLOYMENT_TARGET`, else
15.0) or if `LSMinimumSystemVersion` differs from it. A local build on a newer macOS must pass the
host version as the floor, because Homebrew bottles target the OS they were built for, for example:

```bash
MACOSX_DEPLOYMENT_TARGET=27.0 ./libs/build_macos.sh && MACOSX_DEPLOYMENT_TARGET=27.0 ./libs/package_macos.sh
```

## Run

```bash
open deployment/macos-arm64/Proxor.app
```

Config lives at `~/Library/Preferences/proxor` by default (macOS `QStandardPaths::AppConfigLocation`,
confirmed at runtime as `Install channel: portable` in the app log). The default mixed
(SOCKS5 + HTTP) inbound listens on `127.0.0.1:2080`.

## Notes

- The bundle is ad-hoc signed (`codesign --sign -`) and not notarized.
- If the bundle is ever copied through a download, AirDrop, or a zip, clear the quarantine
  attribute first: `xattr -dr com.apple.quarantine Proxor.app`, or right-click and choose Open.
- Not supported on macOS yet: autorun, in-app self-update (update with
  `brew upgrade --cask proxor`), a DMG installer, and an app icon (`.icns`). Tun Mode and System
  Proxy are supported; see [Tun and System Proxy](#tun-and-system-proxy).
- The menu-bar icon is an app-owned `NSStatusItem`, not `QSystemTrayIcon`: Qt 6.11's tray icon
  crashes on macOS 27 when its menu opens (it reads `-[NSEvent clickCount]` on a system-defined
  event). Settings > Appearance > Tray Icon has a "Colored menu bar icon" checkbox; unchecked, the
  icon is a monochrome template image that macOS tints for the light/dark menu bar (running = both
  arcs solid, stopped = one arc faint).
- Cmd+Q and Dock > Quit run Proxor's normal exit (they stop `proxor_core` too). Exit, Settings and
  About stay in Proxor's own menus; the macOS application menu carries its own Settings... (Cmd+,)
  and About Proxor entries.
- "Windows Classic" is not offered on macOS, and the System theme is the native macOS look,
  following the OS light/dark appearance live: a toolbar with native push buttons, flat tabs and
  headers, rounded fields, and native `NSMenu` popups. The runtime look lives in `src/ui/mac/`
  (`MacPlatform.mm`, `MacLook.cpp`, `MacDialogs.cpp`); the shared `.ui` files are not edited for it.
- Subscription requests send `User-Agent: Proxor/macOS/<version>`, `X-Device-OS: macOS` and
  `X-Device-Model: <hw.model>` (for example `Mac15,6`). The "include computer/user name" options
  do not apply on macOS and are hidden.
