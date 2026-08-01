param(
    [string]$OfficialDat = $(if ($env:FPT_OFFICIAL_DAT) { $env:FPT_OFFICIAL_DAT } else { "" }),
    [string]$VcVars = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = Join-Path $repoRoot "outputs\verification\msvc_asan"
$buildRoot = Join-Path $repoRoot "build\asan"
$exe = Join-Path $buildRoot "faultpolytrace_asan.exe"
New-Item -ItemType Directory -Force -Path $outputRoot, $buildRoot | Out-Null
if (!(Test-Path -LiteralPath $OfficialDat)) { throw "Official test data not found: $OfficialDat" }
if (!(Test-Path -LiteralPath $VcVars)) { throw "Visual C++ environment not found: $VcVars" }

$sources = @(
    "src\app\main.cpp",
    "src\core\AdaptiveBoundary.cpp",
    "src\core\BinaryMask.cpp",
    "src\core\BranchAttribution.cpp",
    "src\core\Grid2D.cpp",
    "src\core\GridMetric.cpp",
    "src\core\LocalPolygonQuality.cpp",
    "src\core\MultiScaleEvidence.cpp",
    "src\core\PhysicalMorphology.cpp",
    "src\core\Polygon.cpp",
    "src\core\PolygonQualityMetrics.cpp",
    "src\core\PolygonVectorizer.cpp",
    "src\core\Segmentation.cpp",
    "src\core\SkeletonGraph.cpp",
    "src\core\SkeletonGraphV2.cpp",
    "src\core\SkeletonGraphV2Attribution.cpp",
    "src\core\Topology.cpp",
    "src\core\FaultTracer.cpp",
    "src\core\FinalBridgeGate.cpp",
    "src\core\TraceOptionAdapters.cpp",
    "src\core\TraceOptionsValidation.cpp",
    "src\core\TracePostProcessor.cpp",
    "src\core\WeakContinuityConnector.cpp",
    "src\core\WeakExtensionGate.cpp",
    "src\core\WeakPathSupport.cpp",
    "src\core\WeakTailRestorer.cpp",
    "src\io\AsciiGridIO.cpp",
    "src\io\DiagnosticsIO.cpp"
) | ForEach-Object { '"' + (Join-Path $repoRoot $_) + '"' }
$compileLog = Join-Path $outputRoot "build.log"
$compile = "call `"$VcVars`" && cl /nologo /std:c++17 /EHsc /O1 /Zi /MD /utf-8 /fsanitize=address $($sources -join ' ') /Fe:`"$exe`" /link /INCREMENTAL:NO shell32.lib > `"$compileLog`" 2>&1"
cmd.exe /d /s /c $compile
if ($LASTEXITCODE -ne 0 -or !(Test-Path -LiteralPath $exe)) {
    throw "MSVC AddressSanitizer build failed. See $compileLog"
}
$vcToolsBin = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64"
if (Test-Path -LiteralPath $vcToolsBin) {
    $env:Path = "$vcToolsBin;$env:Path"
}

$resultCsv = Join-Path $outputRoot "official_result.csv"
$runLog = Join-Path $outputRoot "official_run.log"
$env:ASAN_SAVE_DUMPS = Join-Path $outputRoot "asan_failure.dmp"
$arguments = @(
    "--input", $OfficialDat, "--input-format", "geoeast-dat", "--output", $resultCsv,
    "--trace-mode", "ridge-belt", "--threshold-mode", "percentile", "--percentile", "0.98",
    "--close-radius", "1", "--weak-percentile", "0.965", "--weak-connect-radius", "1",
    "--max-weak-component-cells", "20000", "--min-weak-component-aspect", "3",
    "--belt-radius", "3", "--attribute-constrained-belt", "--attribute-envelope-radius", "1",
    "--post-belt-close-radius", "1", "--min-branch-length", "4",
    "--min-junction-branch-length", "2", "--min-junction-branch-mean", "0.94",
    "--max-gap-distance", "12", "--max-gap-angle", "55", "--min-gap-support-ratio", "0.25",
    "--no-weak-continuity", "--no-weak-continuity-second-pass",
    "--weak-continuity-final-object-pass", "--weak-tail-completion",
    "--preserve-observed-skeleton-envelope",
    "--no-separate-branch-belts", "--root-anti-merge", "--root-anti-merge-support-ratio", "0.25",
    "--root-anti-merge-min-part-cells", "0", "--root-anti-merge-min-part-ratio", "0.12",
    "--root-anti-merge-weak-guard-threshold", "0.66", "--root-anti-merge-weak-guard-radius", "0",
    "--residual-percentile", "0.955", "--residual-fill-radius", "4",
    "--residual-edge-fill-radius", "2", "--residual-edge-fill-min-value-ratio", "0.97",
    "--residual-edge-fill-max-patch-span", "5", "--residual-min-cells", "2",
    "--residual-trace-min-length", "6", "--residual-trace-max-cells", "80",
    "--residual-covered-guard-radius", "0", "--no-residual-trace",
    "--min-area", "120", "--simplify-tolerance", "2", "--smooth-iterations", "1"
)
& $exe @arguments *> $runLog
$officialExit = $LASTEXITCODE
$compilerLabel = "MSVC 2019 /fsanitize=address"
$msvcRuntimeStatus = "PASS"
if ($officialExit -ne 0) {
    $msvcRuntimeStatus = "STARTUP_FAILURE_$officialExit"
    $clang = (Get-Command clang++.exe -ErrorAction SilentlyContinue).Source
    if ([string]::IsNullOrWhiteSpace($clang)) {
        throw "MSVC ASan runtime could not start and clang++ fallback is unavailable. See $runLog"
    }
    $clangSources = @($sources | ForEach-Object { $_.Trim('"') })
    & $clang -std=c++17 -O1 -g -fsanitize=address -fno-omit-frame-pointer @clangSources -lshell32 -o $exe *> (Join-Path $outputRoot "clang_asan_build.log")
    if ($LASTEXITCODE -ne 0) {
        throw "LLVM AddressSanitizer fallback build failed."
    }
    $compilerLabel = "LLVM-MinGW clang++ AddressSanitizer fallback"
    & $exe @arguments *> $runLog
    $officialExit = $LASTEXITCODE
}
if ($officialExit -ne 0) { throw "ASan official-data run failed. See $runLog" }
$runText = Get-Content -LiteralPath $runLog -Raw -ErrorAction SilentlyContinue
if ($runText -match 'AddressSanitizer|ERROR:') {
    throw "AddressSanitizer reported a memory error. See $runLog"
}
$expectedHash = "2911D696F880E1173F6633E6B8A4587316FDC7DFA19109CE8EB28083A076ED3A"
$actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resultCsv).Hash
if ($actualHash -ne $expectedHash) {
    throw "ASan result hash mismatch. Expected $expectedHash, actual $actualHash"
}

$badInput = Join-Path $outputRoot "malformed.dat"
Set-Content -LiteralPath $badInput -Value "not a GeoEast grid" -Encoding ASCII
$previousErrorAction = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $exe --input $badInput --input-format geoeast-dat --output (Join-Path $outputRoot "must_not_exist.csv") *> (Join-Path $outputRoot "malformed_input.log")
$malformedExit = $LASTEXITCODE
$ErrorActionPreference = $previousErrorAction
if ($malformedExit -eq 0) { throw "Malformed input was not rejected under ASan." }

[ordered]@{
    status = "PASS"
    compiler = $compilerLabel
    sanitizer = "AddressSanitizer"
    msvcRuntimeStatus = $msvcRuntimeStatus
    officialDataExitCode = $officialExit
    malformedInputRejected = $true
    keptPolygonCount = 200
    resultSha256 = $actualHash
    asanDumpCreated = (Test-Path -LiteralPath $env:ASAN_SAVE_DUMPS)
    note = "No AddressSanitizer memory-safety report was observed in the covered CLI paths. MSVC ASan does not provide LeakSanitizer."
    checkedAt = (Get-Date).ToString("o")
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $outputRoot "asan_report.json") -Encoding UTF8

Write-Host "MSVC AddressSanitizer validation passed: $outputRoot"
