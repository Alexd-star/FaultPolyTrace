[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$InstallerPath,
    [Parameter(Mandatory = $true)][string]$PackageDir,
    [Parameter(Mandatory = $true)][string]$OfficialDat,
    [string]$ExpectedCsvSha256 = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$installer = (Resolve-Path -LiteralPath $InstallerPath).Path
$package = (Resolve-Path -LiteralPath $PackageDir).Path
$dat = (Resolve-Path -LiteralPath $OfficialDat).Path
$expectedIdentity = Get-Content -LiteralPath (Join-Path $package "构建与依赖清单.json") -Raw | ConvertFrom-Json
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$smokeRoot = Join-Path $repoRoot "release\installer_smoke\$stamp"
$installDir = Join-Path $smokeRoot "installed"
$outputDir = Join-Path $smokeRoot "output"
New-Item -ItemType Directory -Force -Path $installDir, $outputDir | Out-Null
$resultCsv = Join-Path $outputDir "official_result.csv"
$diagnostics = Join-Path $outputDir "official_result.diagnostics.json"
$topology = Join-Path $outputDir "official_result.topology.csv"
$branches = Join-Path $outputDir "official_result.branches.csv"
$uninstallAttempted = $false

try {
    $install = Start-Process -FilePath $installer -ArgumentList @(
        "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-", "/DIR=$installDir"
    ) -Wait -PassThru
    if ($install.ExitCode -ne 0) { throw "Installer exited with code $($install.ExitCode)." }

    $gui = Join-Path $installDir "FaultPolyTrace.exe"
    $cli = Join-Path $installDir "bin\faultpolytrace.exe"
    $installedIdentityPath = Join-Path $installDir "构建与依赖清单.json"
    foreach ($required in @($gui, $cli, $installedIdentityPath)) {
        if (!(Test-Path -LiteralPath $required -PathType Leaf)) { throw "Installed file missing: $required" }
    }
    $installedIdentity = Get-Content -LiteralPath $installedIdentityPath -Raw | ConvertFrom-Json
    if ($installedIdentity.buildId -ne $expectedIdentity.buildId) {
        throw "Installed buildId does not match package buildId."
    }
    $forbidden = @(Get-ChildItem -LiteralPath $installDir -Recurse -File | Where-Object {
        $_.Extension.ToLowerInvariant() -in @(".cpp", ".h", ".hpp", ".pro", ".vcxproj", ".key", ".pem", ".lic")
    })
    if ($forbidden.Count -gt 0) { throw "Installer contains forbidden development or secret files." }

    $cliArgs = @(
        "--input", $dat, "--input-format", "geoeast-dat", "--output", $resultCsv,
        "--diagnostics", $diagnostics, "--topology-output", $topology, "--branches-output", $branches,
        "--trace-mode", "ridge-belt", "--threshold-mode", "percentile", "--percentile", "0.98",
        "--min-area", "120", "--close-radius", "1", "--weak-percentile", "0.965",
        "--weak-connect-radius", "1", "--max-weak-component-cells", "20000",
        "--min-weak-component-aspect", "3", "--belt-radius", "3", "--attribute-constrained-belt",
        "--attribute-envelope-radius", "1", "--post-belt-close-radius", "1",
        "--min-branch-length", "4", "--min-junction-branch-length", "2",
        "--min-junction-branch-mean", "0.94", "--max-gap-distance", "12", "--max-gap-angle", "55",
        "--min-gap-support-ratio", "0.25", "--no-separate-branch-belts", "--root-anti-merge",
        "--root-anti-merge-support-ratio", "0.25", "--root-anti-merge-min-part-cells", "0",
        "--root-anti-merge-min-part-ratio", "0.12", "--root-anti-merge-weak-guard-threshold", "0.66",
        "--root-anti-merge-weak-guard-radius", "0", "--residual-percentile", "0.955",
        "--residual-fill-radius", "4", "--residual-edge-fill-radius", "2",
        "--residual-edge-fill-min-value-ratio", "0.97", "--residual-edge-fill-max-patch-span", "5",
        "--residual-min-cells", "2", "--residual-trace-min-length", "6",
        "--residual-trace-max-cells", "80", "--residual-covered-guard-radius", "0",
        "--no-residual-trace", "--simplify-tolerance", "2", "--smooth-iterations", "1"
    )
    & $cli @cliArgs | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Installed CLI failed with code $LASTEXITCODE." }
    $diag = Get-Content -LiteralPath $diagnostics -Raw | ConvertFrom-Json
    if ($diag.results.keptPolygonCount -ne 214) { throw "Expected 214 kept polygons." }
    if ($diag.topology.openRings -ne 0 -or $diag.topology.selfIntersectingPolygons -ne 0 -or $diag.topology.polygonIntersections -ne 0) {
        throw "Installed CLI topology gate failed."
    }
    $resultHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resultCsv).Hash
    if (![string]::IsNullOrWhiteSpace($ExpectedCsvSha256) -and $resultHash -ne $ExpectedCsvSha256.ToUpperInvariant()) {
        throw "Installed CLI output hash does not match the accepted baseline."
    }
    [ordered]@{
        status = "PASS"
        buildId = $installedIdentity.buildId
        installerSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash
        resultSha256 = $resultHash
        keptPolygons = $diag.results.keptPolygonCount
        elapsedMs = $diag.timing.elapsedMs
        forbiddenFileCount = $forbidden.Count
        topology = $diag.topology
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $smokeRoot "installer_smoke_summary.json") -Encoding UTF8
} finally {
    $uninstaller = Join-Path $installDir "unins000.exe"
    if (Test-Path -LiteralPath $uninstaller -PathType Leaf) {
        $uninstallAttempted = $true
        $uninstall = Start-Process -FilePath $uninstaller -ArgumentList @(
            "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART"
        ) -Wait -PassThru
        if ($uninstall.ExitCode -ne 0) { throw "Uninstaller exited with code $($uninstall.ExitCode)." }
    }
}
if (!$uninstallAttempted) { throw "Uninstaller was not found." }
if (Test-Path -LiteralPath (Join-Path $installDir "FaultPolyTrace.exe") -PathType Leaf) {
    throw "FaultPolyTrace.exe remains after uninstall."
}
Write-Host "Installer smoke test passed: $smokeRoot"
