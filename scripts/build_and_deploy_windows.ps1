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
$repoRoot = Split-Path -Parent $scriptDir

& (Join-Path $scriptDir "build_windows.ps1") -BuildDir $BuildDir -Configuration $Configuration -Version:$Version -Clean:$Clean -SkipGoTests:$SkipGoTests
& (Join-Path $scriptDir "deploy_windows.ps1") -BuildDir $BuildDir -Version:$Version -BuildGo -SkipQtDeploy:$SkipQtDeploy -SkipPublicRes:$SkipPublicRes

$resolvedVersion = if ($Version) { $Version } else { (Get-Content (Join-Path $repoRoot "VERSION.txt") -TotalCount 1).Trim() }
$zipName = "proxor-$resolvedVersion-windows64.zip"
$deploymentDir = Join-Path $repoRoot "deployment"
$sourceDir = Join-Path $deploymentDir "windows64"
$zipPath = Join-Path $deploymentDir $zipName

New-Item -ItemType Directory -Force -Path $deploymentDir | Out-Null

if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}

Compress-Archive -Path "$sourceDir\*" -DestinationPath $zipPath

Write-Host ""
Write-Host "Package ready: $zipPath"
