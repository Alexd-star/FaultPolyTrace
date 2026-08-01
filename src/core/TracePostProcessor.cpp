#include "TracePostProcessor.h"

#include "SkeletonGraph.h"
#include "TraceOptionAdapters.h"
#include "TraceOptions.h"
#include "TraceResult.h"

#include <cmath>
#include <utility>
#include <vector>

namespace fpt {
namespace {

void runLocalPolygonQualityStage(
    const Grid2D& grid,
    const TraceOptions& options,
    TraceResult& result) {
    if (!options.localPolygonQuality && !options.localPolygonQualityShadow) {
        return;
    }
    const LocalPolygonQualityOptions localOptions =
        makeLocalPolygonQualityOptions(options);
    std::vector<double> originalAreas;
    std::vector<int> originalCellCounts;
    originalAreas.reserve(result.polygons.size());
    originalCellCounts.reserve(result.polygons.size());
    for (const Polygon& polygon : result.polygons) {
        originalAreas.push_back(polygon.area);
        originalCellCounts.push_back(polygon.cellCount);
    }
    LocalPolygonQualityResult localResult = fpt::applyLocalPolygonQuality(
        grid,
        result.polygons,
        result.effectiveThreshold,
        localOptions);
    result.localPolygonQualityStats = localResult.stats;
    result.localPolygonQualityEvidence = std::move(localResult.evidence);
    if (options.localPolygonQualityShadow) {
        return;
    }
    result.polygons = std::move(localResult.polygons);
    for (std::size_t index = 0; index < result.polygons.size(); ++index) {
        Polygon& polygon = result.polygons[index];
        const bool repaired =
            polygon.area + 1e-9 < originalAreas[index] ||
            polygon.cellCount < originalCellCounts[index];
        if (!repaired || !polygon.kept) {
            continue;
        }
        if (polygon.cellCount < options.minCells ||
            polygon.area < options.minArea) {
            polygon.kept = false;
            ++result.localPolygonQualityStats.postRepairFilteredCount;
        }
    }
}

void evaluateShapeCandidates(
    const Grid2D& grid,
    const BinaryMask& attributeMask,
    const TraceOptions& options,
    TraceResult& result) {
    if (!options.multiScaleEvidenceShadow &&
        !options.adaptiveBoundaryShadow &&
        !options.adaptiveBoundaryApply) {
        return;
    }
    MultiScaleEvidenceResult evidence = computeMultiScaleEvidence(
        grid,
        &attributeMask,
        makeMultiScaleEvidenceOptions(options));
    if (options.traceMode == TraceMode::RidgeBelt) {
        const BinaryMask shadowSkeleton = skeletonizeZhangSuen(attributeMask);
        evaluateMultiScaleEvidenceAgainstSkeleton(
            grid, shadowSkeleton, 35.0, evidence);
    }
    result.multiScaleEvidenceShadowEvaluated = true;
    result.multiScaleEvidenceSummary = evidence.summary;
    if (!options.adaptiveBoundaryShadow && !options.adaptiveBoundaryApply) {
        return;
    }
    BinaryMask weakSupportMask(grid.nx(), grid.ny());
    const double weakSupportThreshold =
        result.weakThresholdInfo.threshold;
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            const double value = grid.value(x, y);
            if (std::isfinite(value) && value >= weakSupportThreshold) {
                weakSupportMask.set(x, y, true);
            }
        }
    }
    AdaptiveBoundaryResult boundary = evaluateAdaptiveBoundaries(
        grid,
        attributeMask,
        result.polygons,
        &evidence,
        makeAdaptiveBoundaryOptions(
            options,
            result.effectiveThreshold,
            weakSupportThreshold),
        &weakSupportMask);
    result.adaptiveBoundaryShadowEvaluated = true;
    result.adaptiveBoundarySummary = boundary.summary;
    result.adaptiveBoundaryEvidence = std::move(boundary.evidence);
    if (options.adaptiveBoundaryApply) {
        AdaptiveBoundaryCommitResult committed =
            commitAdaptiveBoundaryCandidates(
                result.polygons,
                result.adaptiveBoundaryEvidence);
        result.polygons = std::move(committed.polygons);
        result.adaptiveBoundaryCommitSummary = committed.summary;
        result.adaptiveBoundaryApplied =
            committed.summary.committedCandidates > 0 &&
            !committed.summary.batchRolledBack;
    }
}

} // namespace

void applyTracePostProcessing(
    const Grid2D& grid,
    const BinaryMask& attributeMask,
    const TraceOptions& options,
    TraceResult& result) {
    runLocalPolygonQualityStage(grid, options, result);
    evaluateShapeCandidates(grid, attributeMask, options, result);
}

} // namespace fpt
