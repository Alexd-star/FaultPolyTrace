param(
    [Parameter(Mandatory = $true)]
    [string]$OfficialDat,
    [int]$Runs = 50,
    [int]$MaxElapsedMs = 5000,
    [double]$MaxWorkingSetGrowthMb = 64.0,
    [int]$MaxHandleGrowth = 32,
    [string]$ExpectedSha256 = "A2A9644692B1E6635B152823CAE192C9C6EF19320AB5EF4273D5B2DB90638ECF",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $repoRoot "build\faultpolytrace.exe"
$outputRoot = Join-Path $repoRoot "examples\outputs\stability"
$tempDir = Join-Path $outputRoot ("run_" + (Get-Date -Format "yyyyMMdd_HHmmss_fff"))
$summaryJson = Join-Path $outputRoot "稳定性摘要.json"
$detailCsv = Join-Path $outputRoot "连续运行明细.csv"

if ($Runs -lt 2) { throw "Runs must be at least 2." }
if (!(Test-Path -LiteralPath $OfficialDat)) { throw "Official data not found: $OfficialDat" }
if (!$SkipBuild) { & (Join-Path $PSScriptRoot "build.ps1") }
if (!(Test-Path -LiteralPath $exe)) { throw "Executable not found: $exe" }

New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
$rows = [System.Collections.Generic.List[object]]::new()

function Invoke-StabilityRun {
    param([int]$Index, [string]$InputPath, [string]$OutputPath)
    $arguments = @(
        "--input", $InputPath, "--input-format", "geoeast-dat", "--output", $OutputPath,
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
        "--root-anti-merge-weak-guard-radius", "0", "--no-residual-trace",
        "--simplify-tolerance", "2", "--smooth-iterations", "1"
    )
    $quotedArguments = $arguments | ForEach-Object {
        if ($_ -match '[\s"]') { '"' + ($_ -replace '"', '\"') + '"' } else { $_ }
    }
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $exe
    $startInfo.Arguments = $quotedArguments -join ' '
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $standardOutputTask = $process.StandardOutput.ReadToEndAsync()
    $standardErrorTask = $process.StandardError.ReadToEndAsync()
    $peakWorkingSet = 0L
    $peakHandles = 0
    while (!$process.HasExited) {
        $process.Refresh()
        $peakWorkingSet = [Math]::Max($peakWorkingSet, $process.PeakWorkingSet64)
        $peakHandles = [Math]::Max($peakHandles, $process.HandleCount)
        Start-Sleep -Milliseconds 25
    }
    $process.WaitForExit()
    $standardOutput = $standardOutputTask.GetAwaiter().GetResult()
    $standardError = $standardErrorTask.GetAwaiter().GetResult()
    $process.Refresh()
    $exitCode = $process.ExitCode
    return [pscustomobject]@{
        Index = $Index
        ExitCode = $exitCode
        PeakWorkingSet64 = $peakWorkingSet
        HandleCount = $peakHandles
        Output = $OutputPath
        StandardOutput = $standardOutput
        StandardError = $standardError
    }
}

try {
    for ($i = 1; $i -le $Runs; $i++) {
        $csv = Join-Path $tempDir ("run_{0:D3}.csv" -f $i)
        $sample = Invoke-StabilityRun -Index $i -InputPath $OfficialDat -OutputPath $csv
        if ($sample.ExitCode -ne 0) {
            throw "第 $i 次连续运行失败，退出码 $($sample.ExitCode)：$($sample.StandardError)"
        }
        $diag = Get-Content -LiteralPath "$csv.diagnostics.json" -Raw | ConvertFrom-Json
        if ([int]$diag.timing.elapsedMs -gt $MaxElapsedMs) {
            throw "第 $i 次运行耗时 $($diag.timing.elapsedMs) ms 超过门限 $MaxElapsedMs ms。"
        }
        $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $csv).Hash
        $rows.Add([pscustomobject]@{
            run = $i
            hash = $hash
            elapsedMs = [int]$diag.timing.elapsedMs
            traceMs = [int]$diag.timing.traceMs
            peakWorkingSetBytes = $sample.PeakWorkingSet64
            peakHandleCount = $sample.HandleCount
            keptPolygons = [int]$diag.results.keptPolygonCount
        })
        Get-ChildItem -LiteralPath $tempDir -Filter ("run_{0:D3}.csv*" -f $i) -File |
            Remove-Item -Force
    }

    $hashes = @($rows | Select-Object -ExpandProperty hash -Unique)
    if ($hashes.Count -ne 1) { throw "结果哈希不一致，连续运行不具备确定性。" }
    if (![string]::IsNullOrWhiteSpace($ExpectedSha256) -and $hashes[0] -ne $ExpectedSha256) {
        throw "连续运行结果虽然一致，但已偏离正式基线哈希。期望 $ExpectedSha256，实际 $($hashes[0])。"
    }

    $firstWindow = @($rows | Select-Object -First ([Math]::Min(5, $rows.Count)))
    $lastWindow = @($rows | Select-Object -Last ([Math]::Min(5, $rows.Count)))
    $firstRss = ($firstWindow | Measure-Object peakWorkingSetBytes -Average).Average
    $lastRss = ($lastWindow | Measure-Object peakWorkingSetBytes -Average).Average
    $rssGrowthMb = ($lastRss - $firstRss) / 1MB
    $firstHandles = ($firstWindow | Measure-Object peakHandleCount -Average).Average
    $lastHandles = ($lastWindow | Measure-Object peakHandleCount -Average).Average
    $handleGrowth = $lastHandles - $firstHandles
    if ($rssGrowthMb -gt $MaxWorkingSetGrowthMb) { throw "Working Set 趋势增长 ${rssGrowthMb} MB 超过门限。" }
    if ($handleGrowth -gt $MaxHandleGrowth) { throw "句柄趋势增长 $handleGrowth 超过门限。" }

    $badInput = Join-Path $tempDir "failure_injection_missing.dat"
    $badOutput = Join-Path $tempDir "failure_injection.csv"
    $failure = Invoke-StabilityRun -Index 0 -InputPath $badInput -OutputPath $badOutput
    if ($failure.ExitCode -eq 0 -or (Test-Path -LiteralPath $badOutput)) {
        throw "失败注入未被正确拒绝，或错误地产生了成功结果。"
    }

    New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
    $rows | Export-Csv -LiteralPath $detailCsv -NoTypeInformation -Encoding UTF8
    function Get-Percentile {
        param([double[]]$Values, [double]$Percentile)
        $sorted = @($Values | Sort-Object)
        if ($sorted.Count -eq 0) { return 0.0 }
        $rank = [Math]::Ceiling($Percentile * $sorted.Count) - 1
        $rank = [Math]::Max(0, [Math]::Min($sorted.Count - 1, $rank))
        return [double]$sorted[$rank]
    }
    $elapsedValues = [double[]]@($rows | Select-Object -ExpandProperty elapsedMs)
    $traceValues = [double[]]@($rows | Select-Object -ExpandProperty traceMs)
    [ordered]@{
        generatedAt = (Get-Date).ToString("o")
        runs = $Runs
        deterministicSha256 = $hashes[0]
        maxElapsedMs = ($rows | Measure-Object elapsedMs -Maximum).Maximum
        p50ElapsedMs = Get-Percentile -Values $elapsedValues -Percentile 0.50
        p95ElapsedMs = Get-Percentile -Values $elapsedValues -Percentile 0.95
        p50TraceMs = Get-Percentile -Values $traceValues -Percentile 0.50
        p95TraceMs = Get-Percentile -Values $traceValues -Percentile 0.95
        maxPeakWorkingSetBytes = ($rows | Measure-Object peakWorkingSetBytes -Maximum).Maximum
        maxPeakHandleCount = ($rows | Measure-Object peakHandleCount -Maximum).Maximum
        workingSetTrendMb = [Math]::Round($rssGrowthMb, 3)
        handleTrend = [Math]::Round($handleGrowth, 3)
        failureInjectionPassed = $true
        note = "该结果证明本轮运行未观察到超门限增长，不等同于形式化证明不存在任何内存泄漏。"
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $summaryJson -Encoding UTF8
    Write-Host "连续运行稳定性检查通过：$Runs 次，SHA256=$($hashes[0])"
    Write-Host "摘要：$summaryJson"
} finally {
    if (Test-Path -LiteralPath $tempDir) {
        Remove-Item -LiteralPath $tempDir -Recurse -Force
    }
}
