# Build Proxor on Windows

This document covers the native Windows GUI build and the companion Go core build used by this repository.

Repository layout note:
- `src/` holds the Qt/C++ application code.
- `assets/` holds Qt resources and release-owned assets.
- `go/` remains the Go workspace root.
- `3rdparty/` and `libs/` remain the vendored/dependency helper roots.
- `scripts/` and `cmake/` remain the build-tooling roots.
- `build/` remains the default native GUI build root.
- `.tmp/` and `.cache/` are the default package/transient output roots for local work.
- `deployment/` remains a supported legacy output location.
- dependency helper scripts remain under `libs/`.

## Prerequisites

- Visual Studio 2022 with the C++ desktop workload
- CMake
- Ninja
- Qt SDK
- Bash available in `PATH` for the helper scripts in `libs/`

The repository includes a Windows build wrapper that loads the Visual Studio toolchain automatically, so you do not need to enter a developer shell by hand.

## Clone the Repository

```powershell
git clone https://github.com/Ogstra/proxor.git --recursive
cd proxor
```

## Qt SDK

The current Windows package built in this repository uses Qt 6.x by default. Qt 5.15 can still be used for compatibility builds, but Qt 6 is the preferred path for the current fork.

If you place the SDK under `./qtsdk/Qt`, CMake now detects it automatically. Otherwise, make sure Qt is available through `CMAKE_PREFIX_PATH` or `Qt6_DIR`.

The bundled `./qtsdk/Qt` layout is intended for the Visual Studio toolchain. Do not configure this repository with MinGW unless you also provide a MinGW-built Qt SDK and matching native dependencies.

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

If you keep Qt under `./qtsdk/Qt`, the default build wrapper is enough.

PowerShell:

```powershell
./scripts/build_windows.ps1
```

Git Bash:

```bash
./scripts/build_windows.sh
```

Both wrappers run the Go tests for `grpc_server` and `proxor_core`, then configure and build the Qt GUI with MSVC + Ninja into `build/` by default. Override it with `-BuildDir` if you want a different build tree.

## Deploy a Local Windows Package

PowerShell:

```powershell
./scripts/deploy_windows.ps1 -BuildGo
```

Git Bash:

```bash
./scripts/deploy_windows.sh -BuildGo
```

This stages `.tmp/deployment/windows64` with:

- `proxor.exe` from the GUI build output
- `proxor_core.exe` and `updater.exe` from the Go workspace
- Qt runtime files via `windeployqt`
- local `.tmp/deployment/public_res` contents when present

The default PowerShell deployment target is now `.tmp/deployment/windows64`, and the shared geodata cache lives under `.tmp/deployment/public_res`. Pass `-OutputDir deployment/windows64` if you explicitly want the old root-level layout.

To do everything in one step:

```powershell
./scripts/build_and_deploy_windows.ps1 -Clean
```

```bash
./scripts/build_and_deploy_windows.sh -Clean
```

If your Qt SDK lives elsewhere, point CMake at it explicitly through the environment before running the wrapper:

```powershell
$env:Qt6_DIR = "C:/Qt/6.7.2/msvc2022_64/lib/cmake/Qt6"
./scripts/build_windows.ps1
```

The GUI executable is produced as `proxor.exe`.

To stage the required Qt runtime files next to the executable:

```powershell
windeployqt proxor.exe
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

The repository uses `.tmp/deployment/windows64` as the default assembled Windows output directory for local testing.

After building:

- `build/proxor.exe` is the GUI artifact
- `.tmp/deployment/windows64/proxor_core.exe` is the backend core
- `.tmp/deployment/windows64/updater.exe` is the updater

If you need a full local test package, copy or deploy the GUI into `.tmp/deployment/windows64` and include the required Qt runtime files and geodata assets.
