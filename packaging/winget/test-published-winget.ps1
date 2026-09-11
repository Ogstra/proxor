[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PreviousTag,
    [Parameter(Mandatory = $true)][string]$ReleaseTag,
    [Parameter(Mandatory = $true)][string]$Workspace
)

$ErrorActionPreference = 'Stop'
$repo = "$env:GITHUB_REPOSITORY"
if ([string]::IsNullOrWhiteSpace($repo)) { throw 'GITHUB_REPOSITORY is required' }

function Get-ReleaseAsset([string]$Tag, [string]$Pattern, [string]$Destination) {
    gh release download $Tag --repo $repo --pattern $Pattern --dir $Destination
    $asset = Get-ChildItem -LiteralPath $Destination -File | Where-Object Name -like $Pattern | Select-Object -First 1
    if ($null -eq $asset) { throw "Missing $Pattern from release $Tag" }
    return $asset
}

$previous = Join-Path $Workspace 'previous'
$current = Join-Path $Workspace 'current'
New-Item -ItemType Directory -Force -Path $previous, $current | Out-Null
$previousZip = Get-ReleaseAsset $PreviousTag '*-winget-x64.zip' $previous
$currentZip = Get-ReleaseAsset $ReleaseTag '*-winget-x64.zip' $current
$sums = Get-ReleaseAsset $ReleaseTag 'SHA256SUMS' $current
$expected = (Select-String -LiteralPath $sums.FullName -Pattern ([regex]::Escape($currentZip.Name))).Line.Split()[0]
if ((Get-FileHash -Algorithm SHA256 $currentZip.FullName).Hash.ToLowerInvariant() -ne $expected.ToLowerInvariant()) { throw 'Current winget archive SHA256SUMS mismatch' }

gh release download $PreviousTag --repo $repo --pattern 'Ogstra.Proxor*.yaml' --dir $previous
gh release download $ReleaseTag --repo $repo --pattern 'Ogstra.Proxor*.yaml' --dir $current
winget settings --enable LocalManifestFiles
winget install --manifest $previous --accept-package-agreements --accept-source-agreements --disable-interactivity
winget upgrade --manifest $current --accept-package-agreements --accept-source-agreements --disable-interactivity

$version = $ReleaseTag.TrimStart('v')
$installed = winget list --id Ogstra.Proxor --exact --accept-source-agreements | Out-String
if ($installed -notmatch [regex]::Escape($version)) { throw "winget list did not report $version" }
$expanded = Join-Path $Workspace 'expanded'
Expand-Archive -LiteralPath $currentZip.FullName -DestinationPath $expanded
# The archive contract is proxor/config/package-manager/winget.
$marker = Get-ChildItem -LiteralPath $expanded -Recurse -File -Filter winget | Where-Object FullName -match 'config[\\/]package-manager[\\/]winget$' | Select-Object -First 1
if ($null -eq $marker) { throw 'Managed winget marker is absent from the published archive' }
$app = Get-ChildItem -LiteralPath $expanded -Recurse -File -Filter proxor.exe | Select-Object -First 1
if ($null -eq $app) { throw 'proxor.exe is absent from the published archive' }
$process = Start-Process -FilePath $app.FullName -ArgumentList '-many' -PassThru
Start-Sleep -Seconds 5
if (Get-Process -Name updater,update-package -ErrorAction SilentlyContinue) { throw 'Published winget package started a self-updater' }
if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }

[pscustomobject]@{ previous_tag = $PreviousTag; release_tag = $ReleaseTag; winget_version = $version; marker = $marker.FullName } |
    ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $Workspace 'winget-publication-report.json')
