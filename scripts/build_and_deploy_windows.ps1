param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$Version,
    [switch]$Clean,
    [switch]$SkipGoTests,
    [switch]$SkipQtDeploy,
    [switch]$SkipPublicRes
)

$ErrorActionPreference = "Stop"

$scriptDir = $PSScriptRoot

& (Join-Path $scriptDir "build_windows.ps1") -BuildDir $BuildDir -Configuration $Configuration -Version:$Version -Clean:$Clean -SkipGoTests:$SkipGoTests
& (Join-Path $scriptDir "deploy_windows.ps1") -BuildDir $BuildDir -Version:$Version -BuildGo -SkipQtDeploy:$SkipQtDeploy -SkipPublicRes:$SkipPublicRes
