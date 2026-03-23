param(
    [string]$BuildDir = "build",
    [string]$GuiExePath,
    [string]$OutputDir = "deployment/windows64",
    [switch]$BuildGo,
    [switch]$SkipQtDeploy,
    [switch]$SkipPublicRes
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$defaultGuiCandidates = @(
    (Join-Path $resolvedBuildDir "proxor.exe"),
    (Join-Path $repoRoot "deployment\windows64\proxor.exe")
)

if (-not $GuiExePath) {
    $GuiExePath = $defaultGuiCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
} elseif (-not [System.IO.Path]::IsPathRooted($GuiExePath)) {
    $GuiExePath = Join-Path $repoRoot $GuiExePath
}

if (-not (Test-Path $GuiExePath)) {
    $candidateList = ($defaultGuiCandidates | ForEach-Object { " - $_" }) -join [Environment]::NewLine
    throw "GUI executable not found.`nChecked:`n$candidateList`nBuild the GUI first or pass -GuiExePath explicitly."
}

$qtBinDir = Join-Path $repoRoot "qtsdk\Qt\bin"
$windeployqt = Join-Path $qtBinDir "windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt.exe was not found at $windeployqt"
}

if ($BuildGo) {
    $versionStandalone = "proxor-" + (Get-Content (Join-Path $repoRoot "proxor_version.txt") -TotalCount 1).Trim()

    if (Test-Path $resolvedOutputDir) {
        Remove-Item -Recurse -Force $resolvedOutputDir
    }
    New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null

    Push-Location (Join-Path $repoRoot "go\cmd\updater")
    try {
        $env:GOOS = "windows"
        $env:GOARCH = "amd64"
        $env:CGO_ENABLED = "0"
        go build -o $resolvedOutputDir -trimpath -ldflags "-w -s"
    } finally {
        Pop-Location
    }

    Push-Location (Join-Path $repoRoot "go\cmd\proxor_core")
    try {
        $env:GOOS = "windows"
        $env:GOARCH = "amd64"
        $env:CGO_ENABLED = "0"
        go build -v -o $resolvedOutputDir -trimpath -ldflags "-w -s -X github.com/Ogstra/proxorlib/proxor_common.Version_proxor=$versionStandalone" -tags "with_gvisor,with_quic,with_dhcp,with_wireguard,with_utls,with_acme,with_clash_api,with_tailscale,with_conntrack,with_grpc"
    } finally {
        Pop-Location
    }
} else {
    New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null
}

Copy-Item $GuiExePath $resolvedOutputDir -Force

$deployedGuiExe = Join-Path $resolvedOutputDir "proxor.exe"

if (-not $SkipQtDeploy) {
    $previousPath = $env:PATH
    try {
        $env:PATH = "$qtBinDir;$previousPath"
        Push-Location $resolvedOutputDir
        try {
            & $windeployqt "proxor.exe" "--no-compiler-runtime" "--no-system-d3d-compiler" "--no-opengl-sw" "--verbose" "2"
        } finally {
            Pop-Location
        }
    } finally {
        $env:PATH = $previousPath
    }
}

foreach ($name in @("translations", "libEGL.dll", "libGLESv2.dll", "Qt6Pdf.dll")) {
    $path = Join-Path $resolvedOutputDir $name
    if (Test-Path $path) {
        Remove-Item -Recurse -Force $path
    }
}

foreach ($dll in @("libcrypto-3-x64.dll", "libssl-3-x64.dll")) {
    $source = Join-Path $qtBinDir $dll
    if (Test-Path $source) {
        Copy-Item $source $resolvedOutputDir -Force
    }
}

if (-not $SkipPublicRes) {
    $publicResDir = Join-Path $repoRoot "deployment\public_res"
    $geositeDb = Join-Path $publicResDir "geosite.db"
    if (-not (Test-Path $geositeDb)) {
        Write-Host "Downloading geo assets to $publicResDir..."
        New-Item -ItemType Directory -Force -Path $publicResDir | Out-Null
        $geoDownloads = @{
            "geoip.dat"    = "https://github.com/Loyalsoldier/v2ray-rules-dat/releases/latest/download/geoip.dat"
            "geosite.dat"  = "https://github.com/v2fly/domain-list-community/releases/latest/download/dlc.dat"
            "geoip.db"     = "https://github.com/SagerNet/sing-geoip/releases/latest/download/geoip.db"
            "geosite.db"   = "https://github.com/SagerNet/sing-geosite/releases/latest/download/geosite.db"
        }
        foreach ($entry in $geoDownloads.GetEnumerator()) {
            $dest = Join-Path $publicResDir $entry.Key
            Write-Host "  -> $($entry.Key)"
            Invoke-WebRequest -Uri $entry.Value -OutFile $dest -UseBasicParsing
        }
        $resPub = Join-Path $repoRoot "res\public"
        if (Test-Path $resPub) {
            Copy-Item (Join-Path $resPub "*") $publicResDir -Force
        }
    }
    if (Test-Path $publicResDir) {
        Copy-Item (Join-Path $publicResDir "*") $resolvedOutputDir -Recurse -Force
    }
}

if (Test-Path $resolvedBuildDir) {
    Get-ChildItem $resolvedBuildDir -Filter "*.pdb" -File -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName (Join-Path $repoRoot "deployment") -Force
    }
}

Write-Host "Deployment ready at $resolvedOutputDir"
Write-Host "GUI: $deployedGuiExe"
