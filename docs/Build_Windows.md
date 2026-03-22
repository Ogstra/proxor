# Build NekoBox on Windows

This document covers the native Windows GUI build and the companion Go core build used by this repository.

## Prerequisites

- Visual Studio 2022 with the C++ desktop workload
- CMake
- Ninja
- Qt SDK
- Bash available in `PATH` for the helper scripts in `libs/`

The commands below are intended to run from a Visual Studio developer shell such as `x64 Native Tools Command Prompt for VS 2022`.

## Clone the Repository

```powershell
git clone https://github.com/Ogstra/nekoray.git --recursive
cd nekoray
```

## Qt SDK

The current Windows package built in this repository uses Qt 6.x. Qt 5.15 can still be used for compatibility builds, but Qt 6 is the preferred path for the current fork.

Make sure the Qt `bin` directory is available through `CMAKE_PREFIX_PATH` or `PATH`.

## Build Native Dependencies

The helper script builds the in-tree native dependencies used by the GUI:

- zxing-cpp `2.0.0`
- yaml-cpp `0.7.0`
- protobuf C++ `21.4`

```powershell
bash ./libs/build_deps_all.sh
```

The produced packages are installed under `libs/deps/built` unless `NKR_PACKAGE` is set.

## Build the GUI

Adjust the Qt path to match your machine:

```powershell
mkdir build
cd build
cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.5.0/msvc2022_64 ..
ninja
```

The GUI executable is produced as `nekobox.exe`.

To stage the required Qt runtime files next to the executable:

```powershell
windeployqt nekobox.exe
```

## Build the Go Core

Build the backend and updater into the deployment directory:

```powershell
set GOOS=windows
set GOARCH=amd64
bash ./libs/build_go.sh
```

For details on workspace layout, source forks, and build tags, see [Build_Core.md](./Build_Core.md).

## Final Windows Package

The repository uses `deployment/windows64` as the assembled Windows output directory for local testing.

After building:

- `build/nekobox.exe` is the GUI artifact
- `deployment/windows64/nekobox_core.exe` is the backend core
- `deployment/windows64/updater.exe` is the updater

If you need a full local test package, copy or deploy the GUI into `deployment/windows64` and include the required Qt runtime files and geodata assets.
