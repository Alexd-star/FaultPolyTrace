#include "TraceOptionsValidation.h"

#include "Grid2D.h"
#include "SimpleRingCompatibility.h"
#include "TraceOptionAdapters.h"
#include "TraceOptions.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fpt {

void validateTraceRequest(
    const Grid2D& grid,
    const TraceOptions& options) {
    if (grid.empty()) {
        throw std::runtime_error("Cannot trace an empty grid.");
    }
    grid.validate();
    if (std::any_of(
            grid.values().begin(),
            grid.values().end(),
            [](double value) { return !std::isfinite(value); })) {
        throw std::runtime_error(
            "Input grid contains a non-finite attribute value.");
    }
    if ((options.thresholdMode == ThresholdMode::Percentile ||
         options.thresholdMode == ThresholdMode::PositivePercentile) &&
        (!std::isfinite(options.percentile) ||
         options.percentile <= 0.0 || options.percentile >= 1.0)) {
        throw std::runtime_error(
            "percentile must be greater than 0 and less than 1 in percentile modes.");
    }
    if (!std::isfinite(options.threshold)) {
        throw std::runtime_error("threshold must be finite.");
    }
    if (!std::isfinite(options.minArea) || options.minArea < 0.0) {
        throw std::runtime_error("minArea must be finite and non-negative.");
    }
    if (options.minCells < 1) {
        throw std::runtime_error("minCells must be at least 1.");
    }
    if (options.openRadius < 0 || options.closeRadius < 0) {
        throw std::runtime_error("Morphology radii must be non-negative.");
    }
    if (!std::isfinite(options.physicalCloseRadiusWorld) ||
        options.physicalCloseRadiusWorld < 0.0) {
        throw std::runtime_error(
            "physicalCloseRadiusWorld must be finite and non-negative.");
    }
    if (options.weakPercentile < 0.0 || options.weakPercentile >= 1.0) {
        throw std::runtime_error(
            "weakPercentile must be 0 or between 0 and 1.");
    }
    if (options.weakConnectRadius < 0) {
        throw std::runtime_error("weakConnectRadius must be non-negative.");
    }
    if (!std::isfinite(options.weakContinuityNoConnectCost) ||
        options.weakContinuityNoConnectCost < 0.0 ||
        options.weakContinuityMaxExactMatchingEndpoints < 2) {
        throw std::runtime_error(
            "Weak continuity global matching options are invalid.");
    }
    if (!std::isfinite(options.weakPathSupportRatio) ||
        options.weakPathSupportRatio < 0.0 ||
        options.weakPathSupportRatio > 1.0) {
        throw std::runtime_error(
            "weakPathSupportRatio must be between 0 and 1.");
    }
    if (options.weakPathMaxUnsupportedRun < 0) {
        throw std::runtime_error(
            "weakPathMaxUnsupportedRun must be non-negative.");
    }
    if (options.localPolygonQuality || options.localPolygonQualityShadow) {
        validateLocalPolygonQualityOptions(
            makeLocalPolygonQualityOptions(options));
    }
    if (options.residualPercentile < 0.0 ||
        options.residualPercentile >= 1.0) {
        throw std::runtime_error(
            "residualPercentile must be 0 or between 0 and 1.");
    }
    if (options.beltRadius < 1) {
        throw std::runtime_error("beltRadius must be at least 1.");
    }
    if (options.postBeltCloseRadius < 0) {
        throw std::runtime_error(
            "postBeltCloseRadius must be non-negative.");
    }
    if (options.attributeEnvelopeRadius < 0) {
        throw std::runtime_error(
            "attributeEnvelopeRadius must be non-negative.");
    }
    if (options.residualFillRadius < 0) {
        throw std::runtime_error("residualFillRadius must be non-negative.");
    }
    if (options.residualEdgeFillRadius < 0) {
        throw std::runtime_error(
            "residualEdgeFillRadius must be non-negative.");
    }
    if (options.residualEdgeFillMinValueRatio < 0.0) {
        throw std::runtime_error(
            "residualEdgeFillMinValueRatio must be non-negative.");
    }
    if (options.residualEdgeFillMaxPatchSpan < 1) {
        throw std::runtime_error(
            "residualEdgeFillMaxPatchSpan must be at least 1.");
    }
    if (options.residualMinCells < 1) {
        throw std::runtime_error("residualMinCells must be at least 1.");
    }
    if (options.residualTraceMaxCells < 0) {
        throw std::runtime_error(
            "residualTraceMaxCells must be non-negative.");
    }
    if (options.residualCoveredGuardRadius < 0) {
        throw std::runtime_error(
            "residualCoveredGuardRadius must be non-negative.");
    }
    if (options.residualDetachedMinCells < 1 ||
        options.residualDetachedMinLength < 1) {
        throw std::runtime_error(
            "residual detached trace minimums must be positive.");
    }
    if (options.minBranchLength < 1) {
        throw std::runtime_error("minBranchLength must be at least 1.");
    }
    if (options.minJunctionBranchLength < 1) {
        throw std::runtime_error(
            "minJunctionBranchLength must be at least 1.");
    }
    if (options.maxGapDistance < 0) {
        throw std::runtime_error("maxGapDistance must be non-negative.");
    }
    if (options.maxGapAngle < 0.0 || options.maxGapAngle > 180.0) {
        throw std::runtime_error(
            "maxGapAngle must be between 0 and 180 degrees.");
    }
    if (options.weakContinuity || options.weakContinuityShadow ||
        options.weakContinuityFinalObjectPass) {
        validateWeakContinuityOptions(makeWeakContinuityOptions(options));
        if (options.weakContinuityReferenceThresholdRatio <= 0.0 ||
            options.weakContinuityReferenceThresholdRatio > 1.0) {
            throw std::runtime_error(
                "weakContinuityReferenceThresholdRatio must be in (0,1].");
        }
    }
    if (options.weakTailCompletion || options.weakTailCompletionShadow ||
        options.weakTailEndpointPortShadow) {
        validateWeakTailOptions(makeWeakTailOptions(
            options, options.weakTailEndpointPortOwnership));
    }
    if (options.simpleRingCompatibility) {
        SimpleRingCompatibilityOptions compatibility;
        compatibility.maximumFillCells =
            options.simpleRingMaximumFillCells;
        compatibility.maximumOpeningCells =
            options.simpleRingMaximumOpeningCells;
        validateSimpleRingCompatibilityOptions(compatibility);
    }
    if (options.weakContinuityFinalObjectPass &&
        (options.traceMode != TraceMode::RidgeBelt ||
         options.separateBranchBelts)) {
        throw std::runtime_error(
            "weakContinuityFinalObjectPass requires RidgeBelt mode with "
            "separateBranchBelts disabled.");
    }
    if (options.rootAntiMergeRadius < 0) {
        throw std::runtime_error(
            "rootAntiMergeRadius must be non-negative.");
    }
    if (options.rootAntiMergeSupportRatio < 0.0 ||
        options.rootAntiMergeSupportRatio > 1.0) {
        throw std::runtime_error(
            "rootAntiMergeSupportRatio must be between 0 and 1.");
    }
    if (options.rootAntiMergeMaxBridgeValue < 0.0) {
        throw std::runtime_error(
            "rootAntiMergeMaxBridgeValue must be non-negative.");
    }
    if (options.rootAntiMergeMinPartCells < 0) {
        throw std::runtime_error(
            "rootAntiMergeMinPartCells must be non-negative.");
    }
    if (options.rootAntiMergeMinPartRatio < 0.0 ||
        options.rootAntiMergeMinPartRatio > 0.5) {
        throw std::runtime_error(
            "rootAntiMergeMinPartRatio must be between 0 and 0.5.");
    }
    if (options.rootAntiMergeWeakGuardThreshold < 0.0) {
        throw std::runtime_error(
            "rootAntiMergeWeakGuardThreshold must be non-negative.");
    }
    if (options.rootAntiMergeWeakGuardRadius < 0) {
        throw std::runtime_error(
            "rootAntiMergeWeakGuardRadius must be non-negative.");
    }
    if (options.localAntiMerge) {
        validateSupportEnvelopeSeparatorOptions(
            makeSupportEnvelopeSeparatorOptions(options, grid));
        validateScopedWeakBridgeOptions(
            makeScopedWeakBridgeOptions(options, grid));
    }
    if (options.simplifyTolerance < 0.0) {
        throw std::runtime_error(
            "simplifyTolerance must be non-negative.");
    }
    if (options.smoothIterations < 0 || options.smoothIterations > 5) {
        throw std::runtime_error(
            "smoothIterations must be between 0 and 5.");
    }
    if (options.branchAttributionCandidateGroups &&
        options.traceMode != TraceMode::RidgeBelt) {
        throw std::runtime_error(
            "branchAttributionCandidateGroups requires RidgeBelt trace mode.");
    }
    if (options.branchAttributionCandidateGroups && !options.rootAntiMerge) {
        throw std::runtime_error(
            "branchAttributionCandidateGroups requires rootAntiMerge.");
    }
    if (options.branchAttributionCandidateGroups &&
        options.separateBranchBelts) {
        throw std::runtime_error(
            "branchAttributionCandidateGroups requires separateBranchBelts=false.");
    }

    const std::size_t gridCellCount =
        static_cast<std::size_t>(grid.nx()) *
        static_cast<std::size_t>(grid.ny());
    constexpr std::size_t kNoAreaFilterMaxGridCells = 250000;
    if (options.minArea == 0.0 &&
        gridCellCount > kNoAreaFilterMaxGridCells) {
        throw std::runtime_error(
            "minArea=0 is disabled for large grids because retaining every fragment can exhaust processing and display resources. Use a small positive minimum area.");
    }
    constexpr std::size_t kCombinedOptionalEnhancementsMaxGridCells = 250000;
    if (gridCellCount > kCombinedOptionalEnhancementsMaxGridCells &&
        options.rootAntiMerge && options.localAntiMerge &&
        options.residualTrace) {
        throw std::runtime_error(
            "rootAntiMerge, localAntiMerge, and residualTrace cannot all be enabled on a large grid. Run the recommended root anti-merge baseline first, then evaluate one optional enhancement at a time.");
    }
}

} // namespace fpt
