[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$OfficialDat,
    [string]$CliPath = "",
    [string]$OutputRoot = ""
)

$ErrorActionPreference = "Stop"
$official = (Resolve-Path -LiteralPath $OfficialDat).Path
$expectedInput = "21AB29456CE19B05CFC5646A1BD67BBA579FF65E30ECA99C3D39DC3C8BFFC07B"
$expectedResult = "A2A9644692B1E6635B152823CAE192C9C6EF19320AB5EF4273D5B2DB90638ECF"

if ([string]::IsNullOrWhiteSpace($CliPath)) {
    $CliPath = Join-Path $PSScriptRoot "..\02_独立软件\FaultPolyTrace_Portable_{{VERSION}}\bin\faultpolytrace.exe"
}
$cli = (Resolve-Path -LiteralPath $CliPath).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path $PSScriptRoot ("复现输出\" + (Get-Date -Format "yyyyMMdd_HHmmss"))
}
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null
$OutputRoot = (Resolve-Path -LiteralPath $OutputRoot).Path

$inputHash = (Get-FileHash -LiteralPath $official -Algorithm SHA256).Hash
if ($inputHash -ne $expectedInput) {
    throw "输入文件不是本作品验收使用的官方 test.dat，SHA256=$inputHash"
}

$csv = Join-Path $OutputRoot "FaultPolyTrace_Official_Result.csv"
$diagnostics = Join-Path $OutputRoot "FaultPolyTrace_Official_Result.diagnostics.json"
$branches = Join-Path $OutputRoot "FaultPolyTrace_Official_Result.branches.csv"
$topology = Join-Path $OutputRoot "FaultPolyTrace_Official_Result.topology.csv"
$arguments = @(
    "--input", $official, "--input-format", "geoeast-dat", "--output", $csv,
    "--diagnostics", $diagnostics, "--branches-output", $branches, "--topology-output", $topology,
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

& $cli @arguments
if ($LASTEXITCODE -ne 0) { throw "核心执行程序退出码为 $LASTEXITCODE" }

$diag = Get-Content -LiteralPath $diagnostics -Raw | ConvertFrom-Json
$resultHash = (Get-FileHash -LiteralPath $csv -Algorithm SHA256).Hash
if ($diag.results.keptPolygonCount -ne 214) { throw "保留多边形数量不一致：$($diag.results.keptPolygonCount)" }
if ($diag.topology.openRings -ne 0 -or $diag.topology.selfIntersectingPolygons -ne 0 -or $diag.topology.polygonIntersections -ne 0) {
    throw "拓扑检查未通过：$($diag.topology | ConvertTo-Json -Compress)"
}
if ($resultHash -ne $expectedResult) { throw "结果哈希不一致：$resultHash" }

$summary = [ordered]@{
    status = "PASS"
    inputSha256 = $inputHash
    resultSha256 = $resultHash
    keptPolygons = $diag.results.keptPolygonCount
    elapsedMs = $diag.timing.elapsedMs
    topology = $diag.topology
    outputDirectory = $OutputRoot
}
$summary | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputRoot "复现结果摘要.json") -Encoding UTF8
Write-Host "官方数据复现通过。"
Write-Host "结果目录：$OutputRoot"

