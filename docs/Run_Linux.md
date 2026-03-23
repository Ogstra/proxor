# Linux Runtime Guide

This document focuses on running locally built or locally staged Linux packages for this repository.

Linux packaging automation is not available right now. Use this guide for manual/local runs only, not as a promise of supported `.deb` or AppImage artifacts.

## Scope

Distribution-specific package managers and third-party distribution channels are intentionally out of scope for this fork documentation. Use the local build artifacts and scripts in this repository instead.

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
