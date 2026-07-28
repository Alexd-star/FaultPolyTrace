$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot "build"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$exe = Join-Path $buildDir "faultpolytrace.exe"
$sources = @(
    (Join-Path $repoRoot "src\app\main.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp"),
    (Join-Path $repoRoot "src\io\AsciiGridIO.cpp"),
    (Join-Path $repoRoot "src\io\DiagnosticsIO.cpp")
)

& g++ -std=c++17 -O2 -Wall -Wextra -pedantic -static -static-libgcc -static-libstdc++ $sources -lshell32 -o $exe
if ($LASTEXITCODE -ne 0) {
    throw "C++ build failed with exit code $LASTEXITCODE."
}
Write-Host "Built $exe"
