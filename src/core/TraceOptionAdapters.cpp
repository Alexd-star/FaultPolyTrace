#include "TraceOptionAdapters.h"

#include "TraceOptions.h"

#include <algorithm>
#include <cmath>

namespace fpt {

LocalPolygonQualityOptions makeLocalPolygonQualityOptions(
    const TraceOptions& options) {
    LocalPolygonQualityOptions local;
    local.widthRatio = options.localQualityWidthRatio;
    local.minExtraCells = options.localQualityMinExtraCells;
    local.supportThresholdRatio = options.localQualitySupportThresholdRatio;
    local.supportBufferCells = options.localQualitySupportBufferCells;
    local.maxAreaLossRatio = options.localQualityMaxAreaLossRatio;
    local.maxStrongCoverageLossRatio =
        options.localQualityMaxStrongCoverageLossRatio;
    local.maxWeakCoverageLossRatio =
        options.localQualityMaxWeakCoverageLossRatio;
    local.maxBackgroundLeakageIncreaseRatio =
        options.localQualityMaxBackgroundLeakageIncreaseRatio;
    local.stabilityThresholdDeltaRatio =
        options.localQualityStabilityThresholdDeltaRatio;
    local.minStabilityJaccard = options.localQualityMinStabilityJaccard;
    local.maxCandidatesPerObject = options.localQualityMaxCandidatesPerObject;
    local.maxTotalCandidates = options.localQualityMaxTotalCandidates;
    local.maxWindowCells = options.localQualityMaxWindowCells;
    local.maxObjectCells = options.localQualityMaxObjectCells;
    local.maxEvidenceRecords = options.localQualityMaxEvidenceRecords;
    if (options.localPolygonQualityShadow && !options.localPolygonQuality) {
        local.minExtraCells = std::max(local.minExtraCells, 24);
        local.bridgeMinCells = std::max(local.bridgeMinCells, 6);
        local.maxCandidatesPerObject = std::min<std::size_t>(
            local.maxCandidatesPerObject, 16);
        local.maxTotalCandidates = std::min<std::size_t>(
            local.maxTotalCandidates, 512);
        local.maxEvidenceRecords = std::min<std::size_t>(
            local.maxEvidenceRecords, 2048);
    }
    return local;
}

WeakContinuityOptions makeWeakContinuityOptions(
    const TraceOptions& options) {
    WeakContinuityOptions continuity;
    continuity.maxDistance = options.weakContinuityMaxDistance;
    continuity.tangentWindow = options.weakContinuityTangentWindow;
    continuity.corridorRadius = options.weakContinuityCorridorRadius;
    continuity.maxCandidatesPerEndpoint =
        options.weakContinuityMaxCandidatesPerEndpoint;
    continuity.maxEndpointAngleDegrees =
        options.weakContinuityMaxEndpointAngle;
    continuity.maxWidthRatio = options.weakContinuityMaxWidthRatio;
    continuity.minAttributeRatio = options.weakContinuityMinAttributeRatio;
    continuity.localContrastRatio = options.weakContinuityLocalContrastRatio;
    continuity.minSupportRatio = options.weakContinuityMinSupportRatio;
    continuity.minMeanSupport = options.weakContinuityMinMeanSupport;
    continuity.maxUnsupportedRun = options.weakContinuityMaxUnsupportedRun;
    continuity.maxPathLengthRatio = options.weakContinuityMaxPathLengthRatio;
    continuity.maxTurnAngleDegrees = options.weakContinuityMaxTurnAngle;
    continuity.endpointExclusionRadius =
        options.weakContinuityEndpointExclusionRadius;
    continuity.thirdPartySkeletonClearanceRadius =
        options.weakContinuityThirdPartySkeletonClearanceRadius;
    continuity.minCompetitionMargin =
        options.weakContinuityMinCompetitionMargin;
    continuity.usePhysicalMetric = options.weakContinuityUsePhysicalMetric;
    continuity.useGlobalMatching = options.weakContinuityUseGlobalMatching;
    continuity.noConnectCost = options.weakContinuityNoConnectCost;
    continuity.maxExactMatchingEndpoints =
        options.weakContinuityMaxExactMatchingEndpoints;
    return continuity;
}

SupportEnvelopeSeparatorOptions makeSupportEnvelopeSeparatorOptions(
    const TraceOptions& options,
    const Grid2D& grid) {
    SupportEnvelopeSeparatorOptions separator;
    separator.enabled = options.localAntiMerge;
    separator.lowThreshold = options.localAntiMergeLowThreshold;
    separator.supportRadiusWorld =
        static_cast<double>(options.localAntiMergeBridgeRadius) *
        std::min(std::abs(grid.dx()), std::abs(grid.dy()));
    separator.minPartCells = options.localAntiMergeMinPartCells;
    separator.minPartRatio = options.localAntiMergeMinPartRatio;
    separator.minRemovedCells = options.localAntiMergeMinBridgeCells;
    separator.minLowRatio = options.localAntiMergeMinLowRatio;
    separator.maxRemovedRatio = std::clamp(
        options.localAntiMergeMinPartRatio, 0.04, 0.10);
    return separator;
}

ScopedWeakBridgeOptions makeScopedWeakBridgeOptions(
    const TraceOptions& options,
    const Grid2D& grid) {
    ScopedWeakBridgeOptions policy;
    policy.enabled = options.localAntiMerge;
    policy.baselineMaxDistance = 12.0;
    policy.expandedMaxDistance = std::min(
        options.weakContinuityMaxDistance, 14);
    policy.splitInfluenceRadiusWorld =
        static_cast<double>(std::max(
            4, options.localAntiMergeBridgeRadius * 4)) *
        std::min(std::abs(grid.dx()), std::abs(grid.dy()));
    policy.supportNeighborhoodRadius = 1;
    return policy;
}

WeakTailOptions makeWeakTailOptions(
    const TraceOptions& options,
    bool requireEndpointPortOwnership) {
    WeakTailOptions tail;
    tail.minCells = options.weakTailMinCells;
    tail.maxCells = options.weakTailMaxCells;
    tail.minAspect = options.weakTailMinAspect;
    tail.minMeanSupportRatio = options.weakTailMinMeanSupportRatio;
    tail.minPeakSupportRatio = options.weakTailMinPeakSupportRatio;
    tail.maxAlignmentAngleDegrees = options.weakTailMaxAlignmentAngle;
    tail.ownershipClearanceRadius = options.weakTailOwnershipClearanceRadius;
    tail.tangentRadius = options.weakTailTangentRadius;
    tail.requireEndpointPortOwnership = requireEndpointPortOwnership;
    tail.junctionClearanceRadius = options.weakTailJunctionClearanceRadius;
    return tail;
}

MultiScaleEvidenceOptions makeMultiScaleEvidenceOptions(
    const TraceOptions& options) {
    MultiScaleEvidenceOptions evidence;
    evidence.minimumConfidence = options.multiScaleEvidenceMinimumConfidence;
    evidence.minimumRidgeResponse =
        options.multiScaleEvidenceMinimumRidgeResponse;
    evidence.maximumSamples = options.multiScaleEvidenceMaximumSamples;
    return evidence;
}

AdaptiveBoundaryOptions makeAdaptiveBoundaryOptions(
    const TraceOptions& options,
    double supportThreshold,
    double weakSupportThreshold) {
    AdaptiveBoundaryOptions boundary;
    boundary.maximumDisplacementWorld =
        options.adaptiveBoundaryMaximumDisplacementWorld;
    boundary.supportThreshold = supportThreshold;
    boundary.weakSupportThreshold = weakSupportThreshold;
    boundary.minimumSupportGain = options.adaptiveBoundaryMinimumSupportGain;
    boundary.maximumSupportLossRatio =
        options.adaptiveBoundaryMaximumSupportLossRatio;
    boundary.maximumBackgroundLeakageIncreaseRatio =
        options.adaptiveBoundaryMaximumLeakageIncreaseRatio;
    boundary.maximumPolygons = options.adaptiveBoundaryMaximumPolygons;
    boundary.maximumVerticesPerPolygon =
        options.adaptiveBoundaryMaximumVerticesPerPolygon;
    return boundary;
}

} // namespace fpt
