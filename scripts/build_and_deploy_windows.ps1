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

$resolvedBuildDirForSymbols = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
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

# --- Symbols -----------------------------------------------------------------
# PDBs are gitignored and overwritten by every build, so without this a crash from
# a shipped release becomes permanently undiagnosable. Keyed by SHA256 rather than
# version number: the same version can be rebuilt and produce a different binary.
#
# NOTE: the name must NOT end in "windows64.zip". updateArchiveSuffixes() in
# go/grpc_server/update.go matches release assets by that suffix, and would offer
# debug symbols to users as an application update.
$symbolsStage = Join-Path $deploymentDir "symbols-stage"
$symbolsZip   = Join-Path $deploymentDir "proxor-$resolvedVersion-symbols.zip"

$symbolFiles = @("app.pdb", "app.exe", "proxor.pdb", "proxor.exe") |
    ForEach-Object { Join-Path $resolvedBuildDirForSymbols $_ } |
    Where-Object { Test-Path $_ }

if ($symbolFiles.Count -eq 0) {
    Write-Warning "No PDBs or binaries found in '$resolvedBuildDirForSymbols'; skipping symbol archive."
} else {
    if (Test-Path $symbolsStage) { Remove-Item -Recurse -Force $symbolsStage }
    New-Item -ItemType Directory -Force -Path $symbolsStage | Out-Null

    $manifest = New-Object System.Collections.Generic.List[string]
    $manifest.Add("Proxor symbols")
    $manifest.Add("version:  $resolvedVersion")
    $manifest.Add("built:    $((Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ'))")
    $manifest.Add("")
    $manifest.Add("SHA256 identifies the exact build. A rebuild of the same version")
    $manifest.Add("produces different hashes and will NOT match a dump from the release.")
    $manifest.Add("")

    foreach ($f in $symbolFiles) {
        Copy-Item $f $symbolsStage
        $h = (Get-FileHash -Algorithm SHA256 $f).Hash
        $manifest.Add(("{0,-14} {1}" -f (Split-Path $f -Leaf), $h))
    }

    Set-Content -Path (Join-Path $symbolsStage "MANIFEST.txt") -Value $manifest

    if (Test-Path $symbolsZip) { Remove-Item -Force $symbolsZip }
    Compress-Archive -Path "$symbolsStage\*" -DestinationPath $symbolsZip
    Remove-Item -Recurse -Force $symbolsStage

    Write-Host "Symbols ready: $symbolsZip"
    Write-Host "  Attach this to the GitHub release -- deployment/ is gitignored, so it is local only."
}
