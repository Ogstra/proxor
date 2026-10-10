# Build Proxor on macOS

This document covers local Apple Silicon and Intel builds of the GUI application.

## Status

Releases carry `proxor-<version>-macos-arm64.zip`, built by the `package-macos` job of
`.github/workflows/build-proxor-cmake.yml` on the GitHub `macos-15` Apple Silicon runner. Apple
Silicon only, macOS 15 (Sequoia) or later. The app is ad-hoc signed and not notarized, so the zip
is not meant to be opened by double-click after a browser download (Gatekeeper blocks it): install
through Homebrew instead. `./libs/build_macos.sh` and `./libs/package_macos.sh` reproduce the same
build locally. macOS releases are published as prereleases.

Intel Macs: a release may also carry `proxor-<version>-macos-x86_64.zip`, built by the
`Build macOS x86_64 app` job (`package-macos-intel`) on the GitHub `macos-15-intel` runner with the
official Qt 6.7.2 (aqtinstall). It is Intel only (not a universal binary) and needs macOS 12
(Monterey) or later. The Intel zip is optional for now: a release can be published without it, so
check the release page. The `MACOS_INTEL_REQUIRED` switch in the workflow makes it required later.
The Intel build has been run on a real Intel Mac with macOS 12.

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

The same commands work on Intel Macs once a release carries the Intel zip: the cask picks the zip
for the Mac's architecture (Intel needs macOS 12, Apple Silicon macOS 15). Homebrew itself treats
Intel Macs as unsupported (Tier 3, no new bottles) and says it will stop running on Intel in or
after September 2027 (Homebrew Support Tiers page, read 2026-10-07). The cask only downloads our
zip, so it still works, but without Homebrew support. Without Homebrew, download
`proxor-<version>-macos-x86_64.zip` from the release page and run
`xattr -dr com.apple.quarantine Proxor.app`.

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
- **Defaults.** Like on Windows, Tun Mode is on by default on macOS and System Proxy is off. This
  applies to new configurations only: an existing configuration keeps the modes it remembers (it
  is not migrated), and turning Tun Mode on once makes it remembered from then on. Neither an
  error, a stopped service nor a declined install ever turns Tun Mode off for you; only your own
  switch (or Remove) does.
- **Tun and System Proxy follow the connection.** Stopping the profile (Stop, or the toolbar
  button) pauses both: Tun stops routing and your previous proxy settings come back, while both
  switches stay on. Starting a profile resumes them. Restart Proxy and switching profiles do not
  touch them, and a quick Stop then Start does nothing in between. The switches respond
  immediately (the service works in the background). Quitting Proxor restores your proxy settings.
- **Tun at launch.** When Tun (or System Proxy) is remembered and the service is missing, outdated
  or does not accept this user yet, which is the case on the first launch and after every
  `brew upgrade` (Homebrew removes the service), Proxor connects without Tun right away and then
  asks once to install the service (one administrator password prompt). If you press Cancel, or
  the install fails, it stays connected without Tun and says so; Tun can be turned on later from
  the toolbar or from Settings > Tun settings. It asks at most once per launch, and asks again at
  the next launch. If the service IS installed but Tun cannot start at launch while it is
  remembered, Proxor does not connect until Tun works or you turn Tun Mode off ("Tun could not
  start: ... Turn off Tun Mode to connect without it.").
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
  is on in Tun settings; macOS per-interface DNS can bypass the Tun DNS hijack; known VPN clients
  (tailscaled, openvpn, wireguard-go, WireGuard) are sent direct automatically, while external
  cores (for example hysteria) are excluded automatically only in single-core Tun, which macOS does
  not use; on Macs with several users each user
  installs once, and System Proxy is machine-wide.

## Desktop integration

- **Start with system.** Settings > Start with system writes a user LaunchAgent,
  `~/Library/LaunchAgents/io.github.Ogstra.Proxor.autostart.plist`, that runs
  `open -a <Proxor.app> --args -tray` at login (plus `-appdata <dir>` for a custom data folder), so
  Proxor starts in the menu bar with the window hidden. macOS lists it in System Settings >
  General > Login Items & Extensions, where it can be turned off; when it is, Settings shows
  "Start with system is turned off in System Settings > General > Login Items & Extensions." with a
  button that opens Login Items. The agent takes effect at your next login. Turning the option off
  removes the file. `brew upgrade --cask proxor` keeps it; `brew uninstall --zap` removes it. If the
  agent points at another copy of Proxor, Settings says so and turning the option on here moves it.
- **Updates.** The update check finds new versions of the zip for your Mac's architecture
  (`proxor-<version>-macos-arm64.zip` or `proxor-<version>-macos-x86_64.zip`). An Intel Mac sees an
  update only when that release has the Intel zip. Every
  macOS release is a prerelease, so macOS always includes prereleases. A Homebrew install
  (`/Applications/Proxor.app` or `~/Applications/Proxor.app` with the cask present in Caskroom)
  shows `brew upgrade --cask proxor` with a Copy button, and the hint to run `brew update` first if
  Homebrew says Proxor is already up to date. Any other copy shows the zip name and the release
  page, and says to quit Proxor and replace Proxor.app.
- **In-app update.** A Proxor.app installed from the zip (not Homebrew) in a folder you can write
  to, such as `~/Applications`, updates itself. The update dialog then shows
  `Download and Restart`. Proxor downloads the zip into a hidden `.proxor-update` folder beside Proxor.app and
  checks it against the release's `SHA256SUMS` (this proves it is the file the release published,
  the same as the Windows in-app update). Proxor quits and
  `Proxor.app/Contents/Resources/update/proxor-app-update.sh`, run from a temporary copy, waits for
  it, extracts the new app with `ditto` beside the old one, checks the bundle id, the ad-hoc
  signature, the architecture and the minimum macOS, clears the quarantine flag if present, swaps
  the two apps with renames, starts the new one with the same arguments, and deletes the old copy
  only once the new one is still running after a few seconds. Otherwise it puts the old app back
  and starts it. If the Mac stops during the swap, the previous version stays beside the app as the
  hidden `.Proxor.app.previous.<n>` folder; rename it back to Proxor.app.
  If an update fails, the next start shows "Update failed" with the reason and a
  `Download manually` button, and the installed version keeps working. If macOS blocks the swap of an app in
  `/Applications`, this is the same failure dialog.
  Still updated by hand: Homebrew (`brew upgrade --cask proxor`), a read-only folder (for example
  `/Applications` for a standard user), a translocated copy, and every copy of Proxor 1.6.14 or
  older (once). macOS ties permissions such as Screen Recording, Location and Local Network to the
  ad-hoc signature, so grant them again after an update (as after `brew upgrade`). The Tun and
  System Proxy service keeps working after an update; if the new version needs a newer service,
  Proxor asks to update it (administrator password) the next time Tun or System Proxy is turned on.
- **Scan QR code from screen.** Needs the Screen Recording permission. The first scan makes macOS
  ask for it; if it is not granted Proxor says so and offers a button that opens System Settings >
  Privacy & Security > Screen & System Audio Recording (plus the image file and clipboard
  alternatives). After allowing Proxor, quit and reopen it. macOS ties the permission to the app
  signature (ad-hoc), so after each update turn Proxor off and on again in that list (or remove it
  with "-" and add it back). The screen is captured through Qt (`QScreen::grabWindow`).
- **Dock icon.** When the window is hidden (closed to the menu bar, or started with `-tray`),
  clicking the Dock icon brings the main window back and raises it. Launching never opens it.
- **macOS 12 (Intel).** Start with system uses the same LaunchAgent. macOS 12 has no Login Items
  status API for it, so the LaunchAgent counts as enabled when the file exists and Settings never
  shows the "turned off in System Settings" note there. The "open Login Items" button opens
  System Preferences > Users & Groups.
- **Theme.** See Notes: Fusion follows the macOS light/dark appearance; the System theme is hidden.

Does NOT work yet on macOS:

- The System (native macOS) theme is not available yet; it is hidden and Fusion is used.
- A DMG installer does not exist yet.

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

## Build for Intel Macs (x86_64, macOS 12)

Prerequisites: Xcode Command Line Tools, CMake, Ninja, Go, and the official Qt 6.7.2 `clang_64`
from aqtinstall (not the Homebrew Qt, whose bottles target the OS they were built on):

```bash
pip install aqtinstall
aqt install-qt mac desktop 6.7.2 clang_64 --outputdir ~/Qt
PROXOR_QT_DIR=~/Qt/6.7.2/macos ./libs/build_macos_intel.sh
PROXOR_MACOS_ARCH=x86_64 MACOSX_DEPLOYMENT_TARGET=12.0 ./libs/package_macos.sh
```

The build script builds x86_64 static zxing-cpp, yaml-cpp and protobuf into
`libs/deps/macos-x86_64` at deployment target 12.0, builds the darwin/amd64 `proxor_core`, builds
the GUI, assembles the bundle, runs `macdeployqt`, thins every Mach-O to x86_64, sets
`LSMinimumSystemVersion` to 12.0, ad-hoc signs, and checks architectures and minimum versions.
Building on an Apple Silicon Mac needs Rosetta 2, because the x86_64 `protoc` built from the
dependencies runs during the build. Output: `deployment/macos-x86_64/Proxor.app` and
`deployment/proxor-<version>-macos-x86_64.zip`.

| Variable | Default | Meaning |
|---|---|---|
| `PROXOR_QT_DIR` | `$QT_ROOT_DIR` (required) | Qt prefix, `.../6.7.2/macos` |
| `MACOSX_DEPLOYMENT_TARGET` | `12.0` | Minimum macOS for the build and `LSMinimumSystemVersion` |
| `DEPS_PREFIX` | `libs/deps/macos-x86_64/built` | Static dependency prefix (a custom one must already exist) |
| `BUILD_DIR` | `build-macos-x86_64` | CMake build tree |
| `SKIP_GO` | unset | Set to `1` to reuse `deployment/macos-amd64/proxor_core` |
| `SKIP_DEPS` | unset | Set to `1` to skip building the dependencies |

Go 1.26 is the last Go release that runs on macOS 12; moving the repository to Go 1.27 would raise
the Intel floor to macOS 13.

### Availability check

```bash
./libs/check_macos_availability.sh
```

Compiles every source at deployment target 12.0 with unguarded-availability warnings as errors and
prints `AVAILABILITY-OK` when clean. Any new API newer than macOS 12 needs
`if (@available(macOS X, *))` with a fallback. This only checks that the code compiles; it does not
run the fallback.

### Testing on Apple Silicon with Rosetta

```bash
arch -x86_64 deployment/macos-x86_64/Proxor.app/Contents/MacOS/Proxor -many -appdata <temporary folder>
```

This runs the Intel build through Rosetta 2, which checks the x86_64 build itself (bundle, signature,
Qt plugins, UI). It does not prove macOS 12 behavior, because the host runs a newer macOS and the
macOS 12 fallbacks never execute, and it does not stand in for real Intel hardware. Use a temporary
`-appdata` folder so your real configuration is untouched. Apple says Rosetta stays available as a
general tool through macOS 27 only.

## Run

```bash
open deployment/macos-arm64/Proxor.app
```

Config lives at `~/Library/Preferences/proxor` by default (macOS `QStandardPaths::AppConfigLocation`,
confirmed at runtime in the app log as `Install channel: homebrew` for the Homebrew cask and
`Install channel: macos-app` for any other copy). The default mixed
(SOCKS5 + HTTP) inbound listens on `127.0.0.1:2080`.

## Notes

- The bundle is ad-hoc signed (`codesign --sign -`) and not notarized.
- If the bundle is ever copied through a download, AirDrop, or a zip, clear the quarantine
  attribute first: `xattr -dr com.apple.quarantine Proxor.app`, or right-click and choose Open.
- Not supported on macOS yet: a DMG installer (see [Updates](#desktop-integration) for how
  updates work). The app icon exists (`proxor.icns`). Start with
  system, Tun Mode and System Proxy are supported; see [Desktop integration](#desktop-integration)
  and [Tun and System Proxy](#tun-and-system-proxy).
- The menu-bar icon is an app-owned `NSStatusItem`, not `QSystemTrayIcon`: Qt 6.11's tray icon
  crashes on macOS 27 when its menu opens (it reads `-[NSEvent clickCount]` on a system-defined
  event). Settings > Appearance > Tray Icon has a "Colored menu bar icon" checkbox; unchecked, the
  icon is a monochrome template image that macOS tints for the light/dark menu bar (running = both
  arcs solid, stopped = one arc faint).
- Cmd+Q and Dock > Quit run Proxor's normal exit (they stop `proxor_core` too). Exit, Settings and
  About stay in Proxor's own menus; the macOS application menu carries its own Settings... (Cmd+,)
  and About Proxor entries.
- "Windows Classic" is not offered on macOS. The System (native macOS) theme is not available yet
  and is hidden on macOS; Fusion is the default and follows the macOS light/dark appearance live
  when the theme Mode is System. Light and Dark force one appearance. Settings > Appearance says
  so. (The native-look code in `src/ui/mac/` is kept for a future System theme.)
- Subscription requests send `User-Agent: Proxor/macOS/<version>`, `X-Device-OS: macOS` and
  `X-Device-Model: <hw.model>` (for example `Mac15,6`). The "include computer/user name" options
  do not apply on macOS and are hidden.
