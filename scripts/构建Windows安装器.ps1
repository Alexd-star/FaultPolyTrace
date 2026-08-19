param(
    [Parameter(Mandatory = $true)]
    [string]$PackageDir,
    [string]$Version = "1.0.0",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")
$resolvedPackage = Resolve-Path -LiteralPath $PackageDir
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "release\installer"
} elseif (![System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path $repoRoot $OutputDir
}
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
$OutputDir = (Resolve-Path -LiteralPath $OutputDir).Path

foreach ($required in @("FaultPolyTrace.exe", "faultpolytrace.ico", "构建与依赖清单.json", "发布包清单.md")) {
    if (!(Test-Path -LiteralPath (Join-Path $resolvedPackage $required) -PathType Leaf)) {
        throw "Verified package file missing: $required"
    }
}
$buildIdentity = Get-Content -LiteralPath (Join-Path $resolvedPackage "构建与依赖清单.json") -Raw | ConvertFrom-Json

$isccCommand = Get-Command iscc -ErrorAction SilentlyContinue
$isccCandidates = @(
    $(if ($isccCommand) { $isccCommand.Source } else { $null }),
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
    $(if (${env:ProgramFiles(x86)}) { Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe" } else { $null })
)
$iscc = $isccCandidates | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) } | Select-Object -First 1
if (!$iscc) { throw "Inno Setup compiler ISCC.exe was not found." }
$iss = Join-Path $repoRoot "packaging\windows\FaultPolyTrace.iss"
& $iscc "/DPackageSource=$resolvedPackage" "/DAppVersion=$Version" "/DInstallerOutput=$OutputDir" $iss | Out-Host
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed." }

$installer = Join-Path $OutputDir "FaultPolyTrace_Setup_v$Version.exe"
if (!(Test-Path -LiteralPath $installer -PathType Leaf)) { throw "Installer was not generated: $installer" }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash
[ordered]@{
    buildId = $buildIdentity.buildId
    version = $Version
    sourceSnapshotSha256 = $buildIdentity.sourceSnapshot.sha256
    packageContentSha256 = $buildIdentity.packageContentSha256
    packageDir = [System.IO.Path]::GetFileName($resolvedPackage.Path.TrimEnd('\', '/'))
    installer = [System.IO.Path]::GetFileName($installer)
    sizeBytes = (Get-Item -LiteralPath $installer).Length
    sha256 = $hash
    generatedAt = (Get-Date).ToString("o")
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDir "installer_manifest.json") -Encoding UTF8
Write-Host "Installer: $installer"
Write-Host "SHA256: $hash"
