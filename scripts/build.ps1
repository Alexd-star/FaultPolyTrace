$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$exe = Join-Path $buildDir "faultpolytrace.exe"
$sources = @(
    (Join-Path $repoRoot "src\app\main.cpp"),
    (Join-Path $repoRoot "src\core\AdaptiveBoundary.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\MultiScaleEvidence.cpp"),
    (Join-Path $repoRoot "src\core\PhysicalMorphology.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\PolygonQualityMetrics.cpp"),
    (Join-Path $repoRoot "src\core\PolygonVectorizer.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2Attribution.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\FinalBridgeGate.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionAdapters.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionsValidation.cpp"),
    (Join-Path $repoRoot "src\core\TracePostProcessor.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp"),
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp"),
    (Join-Path $repoRoot "src\core\WeakExtensionGate.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp"),
    (Join-Path $repoRoot "src\io\AsciiGridIO.cpp"),
    (Join-Path $repoRoot "src\io\DiagnosticsIO.cpp")
)

& g++ -std=c++17 -O2 -Wall -Wextra -pedantic -static -static-libgcc -static-libstdc++ $sources -lshell32 -o $exe
if ($LASTEXITCODE -ne 0) {
    throw "C++ build failed with exit code $LASTEXITCODE."
}
Write-Host "Built $exe"
