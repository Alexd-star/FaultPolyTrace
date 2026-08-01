param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" }),
    [string]$Executable = "",
    [string]$ExpectedSha256 = "2911D696F880E1173F6633E6B8A4587316FDC7DFA19109CE8EB28083A076ED3A"
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
$exportXml = Join-Path $outputRoot "application_verifier_log.xml"
$startedAt = Get-Date
$before = @{}
if (Test-Path -LiteralPath $logsRoot) {
    Get-ChildItem -LiteralPath $logsRoot -File | ForEach-Object { $before[$_.FullName] = $_.LastWriteTimeUtc }
}
try {
    & $appVerifier /verify $targetName
    if ($LASTEXITCODE -ne 0) { throw "Cannot enable Application Verifier Basics." }
    $output = Join-Path $outputRoot "verified_result.csv"
    $runLog = Join-Path $outputRoot "verified_run.log"
    & $Executable --input $OfficialDat --input-format geoeast-dat --output $output *> $runLog
    $targetExit = $LASTEXITCODE
    & $appVerifier -export log -for $targetName -with "To=$exportXml"
    if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $exportXml)) {
        throw "Application Verifier log export failed."
    }
} finally {
    & $appVerifier /n $targetName
}
if ($targetExit -ne 0) { throw "Application Verifier target run failed with exit code $targetExit." }
$actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $output).Hash
if (![string]::IsNullOrWhiteSpace($ExpectedSha256) -and $actualHash -ne $ExpectedSha256) {
    throw "Application Verifier result hash mismatch. Expected $ExpectedSha256, actual $actualHash"
}
[xml]$verifierXml = Get-Content -Raw -LiteralPath $exportXml
$verifierEntries = @($verifierXml.SelectNodes('//*[local-name()="logSession"]/*'))
if ($verifierEntries.Count -ne 0) {
    throw "Application Verifier reported $($verifierEntries.Count) verifier log entrie(s). See $exportXml"
}
$newLogs = @()
if (Test-Path -LiteralPath $logsRoot) {
    $newLogs = @(Get-ChildItem -LiteralPath $logsRoot -File | Where-Object {
        !$before.ContainsKey($_.FullName) -or $_.LastWriteTimeUtc -gt $before[$_.FullName]
    } | Select-Object -ExpandProperty FullName)
}
$logDetails = @($newLogs | ForEach-Object {
    $file = Get-Item -LiteralPath $_
    [ordered]@{
        path = $file.FullName
        length = $file.Length
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash
    }
})
[ordered]@{
    status = "PASS"
    layers = "Basics"
    target = $Executable
    targetExitCode = $targetExit
    resultSha256 = $actualHash
    expectedSha256 = $ExpectedSha256
    keptPolygonCount = 200
    verifierLogFiles = $logDetails
    verifierXml = $exportXml
    verifierXmlSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $exportXml).Hash
    verifierEntryCount = $verifierEntries.Count
    runLog = $runLog
    startedAt = $startedAt.ToString("o")
    note = "Application Verifier is runtime-coverage based; this report applies to the official-data default CLI path. The output hash is locked to the released 200-polygon baseline."
    checkedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot "application_verifier_report.json") -Encoding UTF8

Write-Host "Application Verifier Basics run completed: $outputRoot"
