param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$Generator,
    [switch]$Clean,
    [switch]$SkipGoTests,
    [switch]$SkipGuiBuild
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio Build Tools 2022."
}

$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsInstall) {
    throw "Visual Studio Build Tools with MSVC were not found."
}

$vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat was not found under $vsInstall."
}

$envDump = & cmd.exe /d /s /c "`"$vcvars`" >nul && set"
if ($LASTEXITCODE -ne 0) {
    throw "Failed to load the Visual Studio developer environment."
}

foreach ($line in $envDump) {
    if ($line -match "^(.*?)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}

if ($Clean -and (Test-Path $buildPath)) {
    Remove-Item -Recurse -Force $buildPath
}

if (-not $Generator) {
    $ninja = Get-Command ninja.exe -ErrorAction SilentlyContinue
    if ($ninja -and $ninja.Source -match '(?i)mingw|msys') {
        $Generator = "NMake Makefiles"
    } else {
        $Generator = "Ninja"
    }
}

if (-not $SkipGoTests) {
    Push-Location (Join-Path $repoRoot "go")
    try {
        go test ./grpc_server/... ./cmd/proxor_core/...
    } finally {
        Pop-Location
    }
}

if (-not $SkipGuiBuild) {
    cmake -S $repoRoot -B $buildPath -G $Generator -DCMAKE_BUILD_TYPE=$Configuration
    cmake --build $buildPath --config $Configuration
}
