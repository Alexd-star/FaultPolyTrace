param(
    [string]$Executable = "",
    [int]$TimeoutSeconds = 15
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Executable)) {
    $Executable = Join-Path $repoRoot "build\faultpolytrace.exe"
}
if (!(Test-Path -LiteralPath $Executable)) {
    throw "FaultPolyTrace executable not found: $Executable"
}

$outputRoot = Join-Path $repoRoot "outputs\verification\cli_robustness"
$caseRoot = Join-Path $outputRoot "cases"
New-Item -ItemType Directory -Force -Path $caseRoot | Out-Null

function Write-CaseFile {
    param([string]$Name, [string]$Content)
    $path = Join-Path $caseRoot $Name
    Set-Content -LiteralPath $path -Value $Content -Encoding ASCII -NoNewline
    return $path
}

$validMatrix = Write-CaseFile "valid_matrix.txt" @"
4 4
0 0
10 10
0 0 0 0
0 1 1 0
0 1 1 0
0 0 0 0
"@
$emptyFile = Write-CaseFile "empty.dat" ""
$headerOnly = Write-CaseFile "header_only.dat" "# Line CMP X Y Value`n"
$malformedRow = Write-CaseFile "malformed_row.dat" "# Line CMP X Y Value`n1 1 invalid 0 1`n"
$nonFiniteCoordinate = Write-CaseFile "nonfinite_coordinate.dat" "# Line CMP X Y Value`n1 1 nan 0 1`n"
$nonFiniteValue = Write-CaseFile "nonfinite_value.dat" "# Line CMP X Y Value`n1 1 0 0 inf`n"
$duplicateSample = Write-CaseFile "duplicate_sample.dat" "# Line CMP X Y Value`n1 1 0 0 1`n1 1 0 0 1`n"
$coordinateConflict = Write-CaseFile "coordinate_conflict.dat" "# Line CMP X Y Value`n1 1 0 0 1`n1 2 10 10 1`n"
$incompleteGrid = Write-CaseFile "incomplete_grid.dat" "# Line CMP X Y Value`n1 1 0 0 1`n1 2 0 10 1`n2 1 10 0 1`n"
$invalidDimensions = Write-CaseFile "invalid_dimensions.txt" "0 4`n0 0`n10 10`n"
$oversizedDimensions = Write-CaseFile "oversized_dimensions.txt" "100000000 2`n0 0`n10 10`n"
$nonFiniteMatrix = Write-CaseFile "nonfinite_matrix.txt" "2 2`n0 0`n10 10`n0 nan 0 0`n"
$unwritableOutput = Join-Path $caseRoot "output_is_directory"
New-Item -ItemType Directory -Force -Path $unwritableOutput | Out-Null

$cases = @(
    [ordered]@{ name = "missing_input"; args = @("--input", (Join-Path $caseRoot "missing.dat"), "--input-format", "geoeast-dat") },
    [ordered]@{ name = "empty_file"; args = @("--input", $emptyFile, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "header_only"; args = @("--input", $headerOnly, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "malformed_row"; args = @("--input", $malformedRow, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "nonfinite_coordinate"; args = @("--input", $nonFiniteCoordinate, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "nonfinite_value"; args = @("--input", $nonFiniteValue, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "duplicate_sample"; args = @("--input", $duplicateSample, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "coordinate_conflict"; args = @("--input", $coordinateConflict, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "incomplete_grid"; args = @("--input", $incompleteGrid, "--input-format", "geoeast-dat") },
    [ordered]@{ name = "invalid_dimensions"; args = @("--input", $invalidDimensions, "--input-format", "matrix") },
    [ordered]@{ name = "oversized_dimensions"; args = @("--input", $oversizedDimensions, "--input-format", "matrix") },
    [ordered]@{ name = "nonfinite_matrix"; args = @("--input", $nonFiniteMatrix, "--input-format", "matrix") },
    [ordered]@{ name = "zero_percentile"; args = @("--input", $validMatrix, "--input-format", "matrix", "--percentile", "0") },
    [ordered]@{ name = "negative_area"; args = @("--input", $validMatrix, "--input-format", "matrix", "--min-area", "-1") },
    [ordered]@{ name = "unknown_option"; args = @("--input", $validMatrix, "--input-format", "matrix", "--unknown-option") },
    [ordered]@{ name = "unwritable_output"; args = @("--input", $validMatrix, "--input-format", "matrix"); output = $unwritableOutput }
)

$results = [System.Collections.Generic.List[object]]::new()
foreach ($case in $cases) {
    $output = if ($case.output) { $case.output } else { Join-Path $caseRoot ($case.name + ".csv") }
    if (Test-Path -LiteralPath $output -PathType Leaf) {
        Remove-Item -LiteralPath $output -Force
    }
    $log = Join-Path $caseRoot ($case.name + ".log")
    $arguments = @($case.args) + @("--output", $output)
    $quoted = $arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }
    $startInfo = [Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.Arguments = $quoted -join ' '
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $completed = $process.WaitForExit($TimeoutSeconds * 1000)
    if (!$completed) {
        $process.Kill()
        $process.WaitForExit()
    }
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    Set-Content -LiteralPath $log -Value ($stdout + $stderr) -Encoding UTF8
    $exitCode = if ($completed) { $process.ExitCode } else { -999 }
    $outputCreated = Test-Path -LiteralPath $output -PathType Leaf
    $passed = $completed -and $exitCode -ne 0 -and !$outputCreated
    $results.Add([ordered]@{
        name = $case.name
        passed = $passed
        exitCode = $exitCode
        timedOut = !$completed
        outputCreated = $outputCreated
        log = $log
    })
}

$failed = @($results | Where-Object { !$_.passed })
$report = [ordered]@{
    format = "FaultPolyTrace.CliRobustnessValidation"
    schemaVersion = 1
    status = if ($failed.Count -eq 0) { "PASS" } else { "FAIL" }
    executable = $Executable
    caseCount = $results.Count
    passedCount = $results.Count - $failed.Count
    failedCount = $failed.Count
    timeoutSeconds = $TimeoutSeconds
    cases = $results
    checkedAt = (Get-Date).ToString("o")
}
$reportPath = Join-Path $outputRoot "cli_robustness_report.json"
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8
if ($failed.Count -ne 0) {
    throw "CLI robustness validation failed for: $($failed.name -join ', '). See $reportPath"
}
Write-Host "CLI robustness validation passed: $($results.Count) cases. Report: $reportPath"
