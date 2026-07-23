[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [Parameter(Mandatory = $true)][string]$ZipPath,
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [string]$OutputBase = ""
)

$ErrorActionPreference = "Stop"
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$zip = (Resolve-Path -LiteralPath $ZipPath).Path
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$identityPath = Join-Path $package "构建与依赖清单.json"
if (!(Test-Path -LiteralPath $identityPath -PathType Leaf)) {
    throw "Package build identity is missing: $identityPath"
}
$identity = Get-Content -LiteralPath $identityPath -Raw | ConvertFrom-Json
if ([string]::IsNullOrWhiteSpace($identity.buildId)) {
    throw "Package buildId is missing."
}
if ([string]::IsNullOrWhiteSpace($OutputBase)) {
    $releaseRoot = Split-Path -Parent $package
    $OutputBase = Join-Path $releaseRoot ($identity.buildId + "_unique_build_manifest")
}

$artifacts = @()
foreach ($item in @(
    @{ role = "release-directory-identity"; path = $identityPath },
    @{ role = "release-zip"; path = $zip },
    @{ role = "windows-installer"; path = $installer }
)) {
    $file = Get-Item -LiteralPath $item.path
    $artifacts += [ordered]@{
        role = $item.role
        path = $file.FullName
        sizeBytes = $file.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
    }
}

$manifest = [ordered]@{
    schemaVersion = 1
    buildId = $identity.buildId
    version = $identity.version
    generatedAt = (Get-Date).ToString("o")
    gitHead = $identity.gitHead
    gitDirty = $identity.gitDirty
    sourceSnapshotSha256 = $identity.sourceSnapshot.sha256
    packageContentSha256 = $identity.packageContentSha256
    binaries = $identity.artifacts
    distributables = $artifacts
}
$jsonPath = $OutputBase + ".json"
$mdPath = $OutputBase + ".md"
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $jsonPath -Encoding UTF8

$lines = @(
    "# FaultPolyTrace Unique Build Manifest",
    "",
    "- Build ID: ``$($manifest.buildId)``",
    "- Version: ``$($manifest.version)``",
    "- Git HEAD: ``$($manifest.gitHead)``",
    "- Git dirty: ``$($manifest.gitDirty)``",
    "- Source snapshot SHA256: ``$($manifest.sourceSnapshotSha256)``",
    "- Package content SHA256: ``$($manifest.packageContentSha256)``",
    "",
    "## Distributables",
    "",
    "| Role | Size (bytes) | SHA256 | Path |",
    "|---|---:|---|---|"
)
foreach ($artifact in $artifacts) {
    $lines += "| $($artifact.role) | $($artifact.sizeBytes) | ``$($artifact.sha256)`` | ``$($artifact.path)`` |"
}
$lines | Set-Content -LiteralPath $mdPath -Encoding UTF8
Write-Host "Build ID: $($manifest.buildId)"
Write-Host "JSON manifest: $jsonPath"
Write-Host "Markdown manifest: $mdPath"
