param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" }),
    [string]$Executable = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $repoRoot "outputs\verification\application_verifier"
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
if ([string]::IsNullOrWhiteSpace($Executable)) {
    $Executable = Join-Path $repoRoot "build\faultpolytrace.exe"
}
$appVerifier = "$env:WINDIR\System32\appverif.exe"
$isAdministrator = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
if (!$isAdministrator) {
    [ordered]@{
        status = "REQUIRES_ELEVATION"
        tool = $appVerifier
        executable = $Executable
        reason = "Application Verifier changes machine-level verifier settings and requires an elevated terminal on this computer."
        command = "powershell -ExecutionPolicy Bypass -File scripts\run_application_verifier.ps1"
        checkedAt = (Get-Date).ToString("o")
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot "application_verifier_report.json") -Encoding UTF8
    Write-Warning "Application Verifier requires an elevated terminal. A reproducible report and command were generated."
    exit 0
}
if (!(Test-Path -LiteralPath $appVerifier)) { throw "Application Verifier is not installed." }
if (!(Test-Path -LiteralPath $Executable)) { throw "Target executable not found: $Executable" }
if (!(Test-Path -LiteralPath $OfficialDat)) { throw "Official test data not found: $OfficialDat" }

$targetName = Split-Path -Leaf $Executable
$logsRoot = Join-Path $env:USERPROFILE "AppVerifierLogs"
$before = @{}
if (Test-Path -LiteralPath $logsRoot) {
    Get-ChildItem -LiteralPath $logsRoot -File | ForEach-Object { $before[$_.FullName] = $_.LastWriteTimeUtc }
}
try {
    & $appVerifier /verify $targetName
    if ($LASTEXITCODE -ne 0) { throw "Cannot enable Application Verifier Basics." }
    $output = Join-Path $outputRoot "verified_result.csv"
    & $Executable --input $OfficialDat --input-format geoeast-dat --output $output `
        --trace-mode ridge-belt --threshold-mode percentile --percentile 0.98 `
        --min-area 120 --root-anti-merge --no-residual-trace
    $targetExit = $LASTEXITCODE
} finally {
    & $appVerifier /n $targetName
}
if ($targetExit -ne 0) { throw "Application Verifier target run failed with exit code $targetExit." }
$newLogs = @()
if (Test-Path -LiteralPath $logsRoot) {
    $newLogs = @(Get-ChildItem -LiteralPath $logsRoot -File | Where-Object {
        !$before.ContainsKey($_.FullName) -or $_.LastWriteTimeUtc -gt $before[$_.FullName]
    } | Select-Object -ExpandProperty FullName)
}
[ordered]@{
    status = "PASS"
    layers = "Basics"
    target = $Executable
    targetExitCode = $targetExit
    verifierLogFiles = $newLogs
    note = "Application Verifier is runtime coverage based; this report applies to the exercised CLI path."
    checkedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot "application_verifier_report.json") -Encoding UTF8

Write-Host "Application Verifier Basics run completed: $outputRoot"
