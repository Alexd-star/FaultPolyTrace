param(
    [string]$Compiler = "g++"
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$testBuildDir = Join-Path $repoRoot "build\tests"
New-Item -ItemType Directory -Force -Path $testBuildDir | Out-Null
$objectBuildDir = Join-Path $testBuildDir "objects"
New-Item -ItemType Directory -Force -Path $objectBuildDir | Out-Null
$objectCache = @{}

$commonFlags = @(
    "-std=c++17",
    "-O2",
    "-Wall",
    "-Wextra",
    "-pedantic"
)

function Get-CppObject {
    param([string]$Source)

    $resolved = (Resolve-Path -LiteralPath $Source).Path
    if ($objectCache.ContainsKey($resolved)) {
        return $objectCache[$resolved]
    }
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($resolved.ToLowerInvariant())
        $hash = ([System.BitConverter]::ToString($sha256.ComputeHash($bytes))).Replace("-", "").Substring(0, 12)
    } finally {
        $sha256.Dispose()
    }
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($resolved) -replace '[^A-Za-z0-9_.-]', '_'
    $object = Join-Path $objectBuildDir "$baseName-$hash.o"
    $compilerOutput = @(& $Compiler @commonFlags "-c" $resolved "-o" $object 2>&1)
    if ($LASTEXITCODE -ne 0) {
        $compilerOutput | ForEach-Object { Write-Host $_ }
        throw "Failed to compile C++ source: $resolved"
    }
    $compilerOutput | ForEach-Object { Write-Host $_ }
    $objectCache[$resolved] = $object
    return $object
}

function Invoke-CppTest {
    param(
        [string]$Name,
        [string[]]$Sources
    )

    $exe = Join-Path $testBuildDir "$Name.exe"
    $objects = @($Sources | ForEach-Object { Get-CppObject -Source $_ })
    & $Compiler @objects "-o" $exe
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to compile C++ test: $Name"
    }

    & $exe
    if ($LASTEXITCODE -ne 0) {
        throw "C++ test failed: $Name"
    }
}

Invoke-CppTest -Name "test_grid_metric" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_grid_metric.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\PhysicalMorphology.cpp")
)

Invoke-CppTest -Name "test_product_defaults" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_product_defaults.cpp")
)

Invoke-CppTest -Name "test_segmentation_threshold_info" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_segmentation_threshold_info.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp")
)

Invoke-CppTest -Name "test_skeleton_graph_v2" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_skeleton_graph_v2.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2.cpp")
)

Invoke-CppTest -Name "test_skeleton_graph_v2_attribution" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_skeleton_graph_v2_attribution.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2Attribution.cpp")
)

Invoke-CppTest -Name "test_weak_extension_gate" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_extension_gate.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\WeakExtensionGate.cpp")
)

Invoke-CppTest -Name "test_multi_scale_evidence" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_multi_scale_evidence.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\MultiScaleEvidence.cpp")
)

Invoke-CppTest -Name "test_adaptive_boundary" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_adaptive_boundary.cpp"),
    (Join-Path $repoRoot "src\core\AdaptiveBoundary.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\MultiScaleEvidence.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\PolygonQualityMetrics.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp")
)

Invoke-CppTest -Name "test_residual_trace" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_residual_trace.cpp"),
    (Join-Path $repoRoot "src\core\AdaptiveBoundary.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\FaultBeltBuilder.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\FinalBridgeGate.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionAdapters.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionsValidation.cpp"),
    (Join-Path $repoRoot "src\core\TracePostProcessor.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\MaskTopology.cpp"),
    (Join-Path $repoRoot "src\core\MultiScaleEvidence.cpp"),
    (Join-Path $repoRoot "src\core\PhysicalMorphology.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\PolygonQualityMetrics.cpp"),
    (Join-Path $repoRoot "src\core\PolygonVectorizer.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SimpleRingCompatibility.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2Attribution.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp"),
    (Join-Path $repoRoot "src\core\WeakExtensionGate.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailTopologyGate.cpp")
)

Invoke-CppTest -Name "test_weak_continuity_connector" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_continuity_connector.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp")
)

Invoke-CppTest -Name "test_final_bridge_gate" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_final_bridge_gate.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\FinalBridgeGate.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp")
)

Invoke-CppTest -Name "test_weak_tail_restorer" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_tail_restorer.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp")
)

Invoke-CppTest -Name "test_weak_tail_topology_gate" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_weak_tail_topology_gate.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\FaultBeltBuilder.cpp"),
    (Join-Path $repoRoot "src\core\MaskTopology.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailTopologyGate.cpp")
)

Invoke-CppTest -Name "test_simple_ring_compatibility" -Sources @(
    (Join-Path $repoRoot "tests_cpp\test_simple_ring_compatibility.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\MaskTopology.cpp"),
    (Join-Path $repoRoot "src\core\SimpleRingCompatibility.cpp")
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
    (Join-Path $repoRoot "src\core\AdaptiveBoundary.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\FaultBeltBuilder.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\MaskTopology.cpp"),
    (Join-Path $repoRoot "src\core\MultiScaleEvidence.cpp"),
    (Join-Path $repoRoot "src\core\PhysicalMorphology.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\PolygonQualityMetrics.cpp"),
    (Join-Path $repoRoot "src\core\PolygonVectorizer.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SimpleRingCompatibility.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2Attribution.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\FinalBridgeGate.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionAdapters.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionsValidation.cpp"),
    (Join-Path $repoRoot "src\core\TracePostProcessor.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp")
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp")
    (Join-Path $repoRoot "src\core\WeakExtensionGate.cpp")
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailTopologyGate.cpp")
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
    (Join-Path $repoRoot "src\core\AdaptiveBoundary.cpp"),
    (Join-Path $repoRoot "src\core\BinaryMask.cpp"),
    (Join-Path $repoRoot "src\core\BranchAttribution.cpp"),
    (Join-Path $repoRoot "src\core\FaultBeltBuilder.cpp"),
    (Join-Path $repoRoot "src\core\FaultTracer.cpp"),
    (Join-Path $repoRoot "src\core\FinalBridgeGate.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionAdapters.cpp"),
    (Join-Path $repoRoot "src\core\TraceOptionsValidation.cpp"),
    (Join-Path $repoRoot "src\core\TracePostProcessor.cpp"),
    (Join-Path $repoRoot "src\core\Grid2D.cpp"),
    (Join-Path $repoRoot "src\core\GridMetric.cpp"),
    (Join-Path $repoRoot "src\core\LocalPolygonQuality.cpp"),
    (Join-Path $repoRoot "src\core\MaskTopology.cpp"),
    (Join-Path $repoRoot "src\core\MultiScaleEvidence.cpp"),
    (Join-Path $repoRoot "src\core\PhysicalMorphology.cpp"),
    (Join-Path $repoRoot "src\core\Polygon.cpp"),
    (Join-Path $repoRoot "src\core\PolygonQualityMetrics.cpp"),
    (Join-Path $repoRoot "src\core\PolygonVectorizer.cpp"),
    (Join-Path $repoRoot "src\core\Segmentation.cpp"),
    (Join-Path $repoRoot "src\core\SimpleRingCompatibility.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraph.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2.cpp"),
    (Join-Path $repoRoot "src\core\SkeletonGraphV2Attribution.cpp"),
    (Join-Path $repoRoot "src\core\Topology.cpp"),
    (Join-Path $repoRoot "src\core\WeakPathSupport.cpp")
    (Join-Path $repoRoot "src\core\WeakContinuityConnector.cpp")
    (Join-Path $repoRoot "src\core\WeakExtensionGate.cpp")
    (Join-Path $repoRoot "src\core\WeakTailRestorer.cpp"),
    (Join-Path $repoRoot "src\core\WeakTailTopologyGate.cpp")
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
