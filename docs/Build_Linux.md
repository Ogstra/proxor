# Build Proxor on Linux

This document covers native Linux builds for the GUI application.

## Status

Linux is currently supported as a manual build path only.

Official release packaging is not available right now:

- no supported `.deb` output
- no supported AppImage output
- no supported CI-produced Linux artifacts

## Prerequisites

- CMake
- Ninja
- A C++17 compiler
- Qt 5.12, Qt 5.15, or a compatible Qt 6 setup
- Bash

Optional in-tree native dependencies can be built from `libs/build_deps_all.sh` if your distribution packages are missing or unsuitable.

## Clone the Repository

```bash
git clone https://github.com/Ogstra/proxor.git --recursive
cd proxor
```

## Basic Build

If your system already provides the required libraries and Qt development packages:

```bash
mkdir build
cd build
cmake -GNinja ..
ninja
```

The GUI executable is produced as `proxor`.

## Build with In-Tree Native Dependencies

If you need local copies of protobuf, yaml-cpp, or zxing-cpp:

```bash
./libs/build_deps_all.sh
mkdir build
cd build
cmake -GNinja ..
ninja
```

By default the build looks for local dependencies in `./libs/deps/built`. That path is appended to `CMAKE_PREFIX_PATH` unless `NKR_DISABLE_LIBS` is set.

## Relevant CMake Options

| Option | Default | Meaning |
|---|---|---|
| `QT_VERSION_MAJOR` | `6` | Selects Qt major version |
| `NKR_NO_EXTERNAL` | unset | Disables all optional native external dependencies |
| `NKR_NO_YAML` | unset | Disables yaml-cpp support |
| `NKR_NO_QHOTKEY` | unset | Disables QHotkey |
| `NKR_NO_ZXING` | unset | Disables zxing-cpp |
| `NKR_NO_GRPC` | unset | Disables gRPC integration |
| `NKR_PACKAGE` | unset | Uses package-oriented dependency layout |
| `NKR_LIBS` | `./libs/deps/built` | Extra dependency search path |
| `NKR_DISABLE_LIBS` | unset | Prevents `NKR_LIBS` from being appended to CMake search paths |

## Go Core

The GUI build does not produce `proxor_core`. For the backend build, see [Build_Core.md](./Build_Core.md).
