# Build Proxor on macOS

This document covers local Apple Silicon builds of the GUI application.

## Status

Local, ad-hoc signed builds only. There is no CI job for macOS yet and no release asset;
`./libs/build_macos.sh` is the only supported way to produce `Proxor.app`, and it has been
verified on Apple Silicon (arm64) only.

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
`proxor_core` and the geodata files next to the GUI executable, runs `macdeployqt`, and
finishes with an ad-hoc `codesign`. Output: `deployment/macos-arm64/Proxor.app`.

Env knobs:

| Variable | Default | Meaning |
|---|---|---|
| `SKIP_GO` | unset | Set to `1` to reuse an already-built `proxor_core` and skip rebuilding it |
| `BUILD_DIR` | `build-macos` | CMake build tree used for the GUI |
| `MACDEPLOYQT` | auto-detected | Override the `macdeployqt` binary to use |

Re-run the script after `brew upgrade qtbase protobuf` (and `rm -rf build-macos` first if
protobuf changed, since a stale build tree can mix headers from two protobuf versions).

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
- Not supported on macOS yet: TUN mode, system proxy, autorun, self-update (update checks
  report no compatible package for this platform), a DMG installer, and an app icon (`.icns`).
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
