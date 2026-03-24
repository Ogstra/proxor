param(
    [string]$BuildDir = "build",
    [string]$LauncherExePath,
    [string]$GuiExePath,
    [string]$OutputDir = ".tmp/deployment/windows64",
    [string]$Version,
    [switch]$BuildGo,
    [switch]$SkipQtDeploy,
    [switch]$SkipPublicRes
)

$ErrorActionPreference = "Stop"
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $true
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $repoRoot $OutputDir }
$deploymentRoot = Split-Path -Parent $resolvedOutputDir
$publicResDir = Join-Path $deploymentRoot "public_res"
$defaultLauncherCandidates = @(
    (Join-Path $resolvedBuildDir "proxor.exe"),
    (Join-Path $repoRoot ".tmp\deployment\windows64\proxor.exe"),
    (Join-Path $repoRoot "deployment\windows64\proxor.exe")
)
$defaultGuiCandidates = @(
    (Join-Path $resolvedBuildDir "app.exe"),
    (Join-Path $repoRoot ".tmp\deployment\windows64\config\runtime\app.exe"),
    (Join-Path $repoRoot "deployment\windows64\config\runtime\app.exe")
)

if (-not $LauncherExePath) {
    $LauncherExePath = $defaultLauncherCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
} elseif (-not [System.IO.Path]::IsPathRooted($LauncherExePath)) {
    $LauncherExePath = Join-Path $repoRoot $LauncherExePath
}

if (-not $GuiExePath) {
    $GuiExePath = $defaultGuiCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
} elseif (-not [System.IO.Path]::IsPathRooted($GuiExePath)) {
    $GuiExePath = Join-Path $repoRoot $GuiExePath
}

if (-not (Test-Path $LauncherExePath)) {
    $candidateList = ($defaultLauncherCandidates | ForEach-Object { " - $_" }) -join [Environment]::NewLine
    throw "Launcher executable not found.`nChecked:`n$candidateList`nBuild the launcher first or pass -LauncherExePath explicitly."
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

function Get-MsvcRuntimeDir {
    if ($env:VCToolsRedistDir) {
        $candidate = Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC143.CRT"
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsInstall) {
            $redistRoot = Join-Path $vsInstall "VC\Redist\MSVC"
            if (Test-Path $redistRoot) {
                $versioned = Get-ChildItem $redistRoot -Directory | Sort-Object Name -Descending
                foreach ($dir in $versioned) {
                    $candidate = Join-Path $dir.FullName "x64\Microsoft.VC143.CRT"
                    if (Test-Path $candidate) {
                        return $candidate
                    }
                }
            }
        }
    }

    return $null
}

if ($BuildGo) {
    $versionStandalone = "proxor-" + $(if ($Version) { $Version } else { (Get-Content (Join-Path $repoRoot "VERSION.txt") -TotalCount 1).Trim() })

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
        if ($LASTEXITCODE -ne 0) {
            throw "Updater go build failed."
        }
    } finally {
        Pop-Location
    }

    Push-Location (Join-Path $repoRoot "go\cmd\proxor_core")
    try {
        $env:GOOS = "windows"
        $env:GOARCH = "amd64"
        $env:CGO_ENABLED = "0"
        go build -v -o $resolvedOutputDir -trimpath -ldflags "-w -s -X github.com/Ogstra/proxorlib/proxor_common.Version_proxor=$versionStandalone" -tags "with_gvisor,with_quic,with_dhcp,with_wireguard,with_utls,with_acme,with_clash_api,with_tailscale,with_conntrack,with_grpc"
        if ($LASTEXITCODE -ne 0) {
            throw "Core go build failed."
        }
    } finally {
        Pop-Location
    }
} else {
    New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null
}

foreach ($stalePath in @(
    (Join-Path $resolvedOutputDir "runtime"),
    (Join-Path $resolvedOutputDir "config\runtime"),
    (Join-Path $resolvedOutputDir "config\plugins"),
    (Join-Path $resolvedOutputDir "generic"),
    (Join-Path $resolvedOutputDir "iconengines"),
    (Join-Path $resolvedOutputDir "imageformats"),
    (Join-Path $resolvedOutputDir "networkinformation"),
    (Join-Path $resolvedOutputDir "platforms"),
    (Join-Path $resolvedOutputDir "sqldrivers"),
    (Join-Path $resolvedOutputDir "styles"),
    (Join-Path $resolvedOutputDir "tls"),
    (Join-Path $resolvedOutputDir "translations"),
    (Join-Path $resolvedOutputDir "proxor_gui.exe"),
    (Join-Path $resolvedOutputDir "app.exe")
)) {
    if (Test-Path $stalePath) {
        try {
            Remove-Item -Recurse -Force $stalePath
        } catch {
            throw "Failed to clean stale deployment path '$stalePath'. Close any running Proxor/app.exe process using that directory and retry."
        }
    }
}

foreach ($pattern in @("Qt6*.dll", "MSVCP140*.dll", "VCRUNTIME*.dll", "libcrypto-*.dll", "libssl-*.dll", "libEGL.dll", "libGLESv2.dll", "Qt6Pdf.dll", "dxcompiler.dll", "dxil.dll")) {
    Get-ChildItem -Path $resolvedOutputDir -Filter $pattern -File -ErrorAction SilentlyContinue | Remove-Item -Force
}

Copy-Item $LauncherExePath (Join-Path $resolvedOutputDir "proxor.exe") -Force
$runtimeDir = Join-Path $resolvedOutputDir "config\runtime"
New-Item -ItemType Directory -Force -Path $runtimeDir | Out-Null
Copy-Item $GuiExePath (Join-Path $runtimeDir "app.exe") -Force

$deployedGuiExe = Join-Path $runtimeDir "app.exe"

if (-not $SkipQtDeploy) {
    $previousPath = $env:PATH
    try {
        $env:PATH = "$qtBinDir;$previousPath"
        Push-Location $runtimeDir
        try {
            & $windeployqt "app.exe" "--no-system-d3d-compiler" "--no-opengl-sw" "--verbose" "2"
            if ($LASTEXITCODE -ne 0) {
                throw "windeployqt failed."
            }
        } finally {
            Pop-Location
        }
    } finally {
        $env:PATH = $previousPath
    }
}

foreach ($name in @("translations", "libEGL.dll", "libGLESv2.dll", "Qt6Pdf.dll", "dxcompiler.dll", "dxil.dll")) {
    $path = Join-Path $runtimeDir $name
    if (Test-Path $path) {
        Remove-Item -Recurse -Force $path
    }
}

foreach ($dll in @("libcrypto-3-x64.dll", "libssl-3-x64.dll")) {
    $source = Join-Path $qtBinDir $dll
    if (Test-Path $source) {
        Copy-Item $source $runtimeDir -Force
    }
}

$msvcRuntimeDir = Get-MsvcRuntimeDir
if ($msvcRuntimeDir) {
    Get-ChildItem $msvcRuntimeDir -Filter "*.dll" -File | ForEach-Object {
        Copy-Item $_.FullName $runtimeDir -Force
    }
    $vcRedistBootstrap = Join-Path $runtimeDir "vc_redist.x64.exe"
    if (Test-Path $vcRedistBootstrap) {
        Remove-Item $vcRedistBootstrap -Force
    }
} else {
    Write-Warning "MSVC runtime DLL directory was not found; leaving vc_redist.x64.exe in runtime."
}

$pluginDeployDir = Join-Path $runtimeDir "plugins"
New-Item -ItemType Directory -Force -Path $pluginDeployDir | Out-Null

$pluginKeepMap = @{
    "platforms"   = @("qwindows.dll")
    "styles"      = @("qmodernwindowsstyle.dll")
    "tls"         = @("qcertonlybackend.dll", "qopensslbackend.dll", "qschannelbackend.dll")
    "iconengines" = @("qsvgicon.dll")
    "imageformats" = @("qsvg.dll")
}

foreach ($pluginType in @("generic", "iconengines", "imageformats", "networkinformation", "platforms", "sqldrivers", "styles", "tls")) {
    $sourceDir = Join-Path $runtimeDir $pluginType
    if (-not (Test-Path $sourceDir)) {
        continue
    }

    if ($pluginKeepMap.ContainsKey($pluginType)) {
        $targetDir = Join-Path $pluginDeployDir $pluginType
        New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
        foreach ($name in $pluginKeepMap[$pluginType]) {
            $source = Join-Path $sourceDir $name
            if (Test-Path $source) {
                Move-Item $source $targetDir -Force
            }
        }
    }

    Remove-Item -Recurse -Force $sourceDir
}

if (-not $SkipPublicRes) {
    $configDir = Join-Path $resolvedOutputDir "config"
    New-Item -ItemType Directory -Force -Path $configDir | Out-Null
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
    }
    if (Test-Path $publicResDir) {
        foreach ($name in @("geoip.dat", "geosite.dat", "geoip.db", "geosite.db")) {
            $source = Join-Path $publicResDir $name
            if (Test-Path $source) {
                Copy-Item $source $configDir -Force
            }
        }
    }
}

if (Test-Path $resolvedBuildDir) {
    Get-ChildItem $resolvedBuildDir -Filter "*.pdb" -File -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName $deploymentRoot -Force
    }
}

Write-Host "Deployment ready at $resolvedOutputDir"
Write-Host "Launcher: $(Join-Path $resolvedOutputDir 'proxor.exe')"
Write-Host "GUI: $deployedGuiExe"
