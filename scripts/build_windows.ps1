param(
    [string]$BuildDir = "build",
    [string]$Configuration = "Release",
    [string]$Generator,
    [string]$Version,
    [switch]$Clean,
    [switch]$SkipGoTests,
    [switch]$SkipGuiBuild
)

$ErrorActionPreference = "Stop"
if ($PSVersionTable.PSVersion.Major -ge 7) {
    $PSNativeCommandUseErrorActionPreference = $true
}

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $repoRoot $BuildDir }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

function Normalize-PathVariable {
    param(
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $Value
    }

    $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    $parts = New-Object System.Collections.Generic.List[string]

    foreach ($segment in ($Value -split ';')) {
        $candidate = $segment.Trim().Trim('"')
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if ($seen.Add($candidate)) {
            $parts.Add($candidate)
        }
    }

    return ($parts -join ';')
}

function Get-CMakeCacheValue {
    param(
        [string]$CachePath,
        [string]$Key
    )

    if (-not (Test-Path $CachePath)) {
        return $null
    }

    $pattern = '^{0}:[^=]+=(.*)$' -f [Regex]::Escape($Key)
    foreach ($line in Get-Content $CachePath) {
        if ($line -match $pattern) {
            return $matches[1]
        }
    }

    return $null
}

function Remove-StaleCMakeCacheIfNeeded {
    param(
        [string]$CachePath,
        [string]$ExpectedRepoRoot,
        [string]$ExpectedBuildPath
    )

    if (-not (Test-Path $CachePath)) {
        return
    }

    $expectedSource = [System.IO.Path]::GetFullPath($ExpectedRepoRoot)
    $expectedBuild = [System.IO.Path]::GetFullPath($ExpectedBuildPath)
    $cacheSource = Get-CMakeCacheValue -CachePath $CachePath -Key "CMAKE_HOME_DIRECTORY"
    $cacheBuild = Get-CMakeCacheValue -CachePath $CachePath -Key "CMAKE_CACHEFILE_DIR"

    $sourceMismatch = $cacheSource -and ([System.StringComparer]::OrdinalIgnoreCase.Compare(
        [System.IO.Path]::GetFullPath($cacheSource),
        $expectedSource
    ) -ne 0)
    $buildMismatch = $cacheBuild -and ([System.StringComparer]::OrdinalIgnoreCase.Compare(
        [System.IO.Path]::GetFullPath($cacheBuild),
        $expectedBuild
    ) -ne 0)

    if ($sourceMismatch -or $buildMismatch) {
        Write-Host "Removing stale CMake cache from $buildPath because it targets a different source/build directory."
        Remove-Item -Recurse -Force $ExpectedBuildPath
    }
}

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

$env:PATH = Normalize-PathVariable $env:PATH

$tempVcvarsScript = Join-Path ([System.IO.Path]::GetTempPath()) ("proxor-vcvars-" + [System.Guid]::NewGuid().ToString("N") + ".cmd")
@(
    '@echo off'
    'set "PATH=' + $env:PATH + '"'
    'call "' + $vcvars + '" >nul'
    'set'
) | Set-Content -Path $tempVcvarsScript -Encoding ASCII

try {
    $envDump = & cmd.exe /d /c $tempVcvarsScript
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to load the Visual Studio developer environment."
    }
} finally {
    if (Test-Path $tempVcvarsScript) {
        Remove-Item -Force $tempVcvarsScript
    }
}

foreach ($line in $envDump) {
    if ($line -match "^(.*?)=(.*)$") {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
    }
}

if ($Clean -and (Test-Path $buildPath)) {
    Remove-Item -Recurse -Force $buildPath
}

$cmakeCachePath = Join-Path $buildPath "CMakeCache.txt"
Remove-StaleCMakeCacheIfNeeded -CachePath $cmakeCachePath -ExpectedRepoRoot $repoRoot -ExpectedBuildPath $buildPath

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
        if ($LASTEXITCODE -ne 0) {
            throw "go test failed."
        }
    } finally {
        Pop-Location
    }
}

if (-not $SkipGuiBuild) {
    $configureArgs = @(
        "-S", $repoRoot,
        "-B", $buildPath,
        "-G", $Generator,
        "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
    )
    if ($Version) {
        $configureArgs += "-DAPP_VERSION_OVERRIDE=$Version"
    } else {
        $configureArgs += "-UAPP_VERSION_OVERRIDE"
    }

    & cmake @configureArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }

    & cmake --build $buildPath --config $Configuration
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed."
    }
}
