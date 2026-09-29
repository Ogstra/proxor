# Build Proxor on macOS

This document covers local Apple Silicon builds of the GUI application.

## Status

Releases carry `proxor-<version>-macos-arm64.zip`, built by the `package-macos` job of
`.github/workflows/build-proxor-cmake.yml` on the GitHub `macos-15` Apple Silicon runner. Apple
Silicon only, macOS 15 (Sequoia) or later. The app is ad-hoc signed and not notarized, so the zip
is not meant to be opened by double-click after a browser download (Gatekeeper blocks it): install
through Homebrew instead. `./libs/build_macos.sh` and `./libs/package_macos.sh` reproduce the same
build locally.

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
- Not supported on macOS yet: TUN mode, system proxy, autorun, in-app self-update (update with
  `brew upgrade --cask proxor`), a DMG installer, and an app icon (`.icns`).
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
