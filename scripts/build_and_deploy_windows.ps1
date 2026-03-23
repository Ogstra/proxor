param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [switch]$Clean,
    [switch]$SkipGoTests,
    [switch]$SkipQtDeploy,
    [switch]$SkipPublicRes
)

$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot

& (Join-Path $scriptDir "build_windows.ps1") -BuildDir $BuildDir -Configuration $Configuration -Clean:$Clean -SkipGoTests:$SkipGoTests
& (Join-Path $scriptDir "deploy_windows.ps1") -BuildDir $BuildDir -BuildGo -SkipQtDeploy:$SkipQtDeploy -SkipPublicRes:$SkipPublicRes
