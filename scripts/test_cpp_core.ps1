param(
    [string]$Compiler = "g++"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$testBuildDir = Join-Path $repoRoot "build\tests"
New-Item -ItemType Directory -Force -Path $testBuildDir | Out-Null

$commonFlags = @(
    "-std=c++17",
    "-O2",
    "-Wall",
    "-Wextra",
    "-pedantic"
)

function Invoke-CppTest {
    param(
        [string]$Name,
        [string[]]$Sources
    )

    $exe = Join-Path $testBuildDir "$Name.exe"
    $compileArgs = $commonFlags + $Sources + @("-o", $exe)
    & $Compiler @compileArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to compile C++ test: $Name"
    }

    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "C++ test failed: $Name"
    }
}

Invoke-CppTest -Name "test_weak_continuity_connector" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_continuity_connector.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp")
)

Invoke-CppTest -Name "test_weak_tail_restorer" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_tail_restorer.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp")
)

Invoke-CppTest -Name "test_weak_path_support" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_path_support.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp")
)

Invoke-CppTest -Name "test_weak_path_support_integration" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_path_support_integration.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp")
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp")
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp")
)

Invoke-CppTest -Name "test_branch_attribution" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_branch_attribution.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp")
)

Invoke-CppTest -Name "test_local_polygon_quality" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_local_polygon_quality.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp")
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp")
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp")
)

Invoke-CppTest -Name "test_topology" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_topology.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp")
)

Invoke-CppTest -Name "test_staged_commit" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_staged_commit.cpp"),
    (Join-Path $repoRoot "geoeast_plugin\bo_runner\StagedCommit.cpp")
)

Write-Host "All C++ core tests passed."
