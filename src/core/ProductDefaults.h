#pragma once

// Product-facing defaults used by the CLI, the standalone application and
// the GeoEast runner. The generic TraceOptions defaults remain suitable for
// embedding the core as a library; released applications initialize from
// this profile before applying user overrides.
namespace fpt::product_defaults {

inline constexpr double threshold = 0.95;
inline constexpr double percentile = 0.98;
inline constexpr double minArea = 120.0;
inline constexpr int closeRadius = 1;
inline constexpr double weakPercentile = 0.965;
inline constexpr int weakConnectRadius = 1;
inline constexpr int beltRadius = 3;
inline constexpr int postBeltCloseRadius = 1;
inline constexpr bool attributeConstrainedBelt = true;
inline constexpr int attributeEnvelopeRadius = 1;
inline constexpr int minBranchLength = 4;
inline constexpr int minJunctionBranchLength = 2;
inline constexpr double minJunctionBranchMean = 0.94;
inline constexpr int maxGapDistance = 12;
inline constexpr double maxGapAngle = 55.0;
inline constexpr double minGapSupportRatio = 0.25;
inline constexpr bool separateBranchBelts = false;
inline constexpr bool rootAntiMerge = true;
inline constexpr int rootAntiMergeRadius = 1;
inline constexpr double rootAntiMergeSupportRatio = 0.25;
inline constexpr double rootAntiMergeMaxBridgeValue = 1e-12;
inline constexpr int rootAntiMergeMinPartCells = 0;
inline constexpr double rootAntiMergeMinPartRatio = 0.12;
inline constexpr double rootAntiMergeWeakGuardThreshold = 0.66;
inline constexpr int rootAntiMergeWeakGuardRadius = 0;
inline constexpr bool localAntiMerge = true;
inline constexpr double localAntiMergeLowThreshold = 0.05;
inline constexpr int localAntiMergeBridgeRadius = 2;
inline constexpr int localAntiMergeMinPartCells = 80;
inline constexpr double localAntiMergeMinPartRatio = 0.08;
inline constexpr int localAntiMergeMinBridgeCells = 6;
inline constexpr double localAntiMergeMinLowRatio = 0.55;
inline constexpr bool residualTrace = false;
inline constexpr bool weakTailTopologyGate = false;
inline constexpr bool simpleRingCompatibility = true;
inline constexpr int simpleRingMaximumFillCells = 25;
inline constexpr int simpleRingMaximumOpeningCells = 128;
inline constexpr double residualPercentile = 0.955;
inline constexpr int residualFillRadius = 4;
inline constexpr int residualEdgeFillRadius = 2;
inline constexpr double residualEdgeFillMinValueRatio = 0.97;
inline constexpr int residualEdgeFillMaxPatchSpan = 5;
inline constexpr int residualMinCells = 2;
inline constexpr int residualTraceMinLength = 6;
inline constexpr int residualTraceMaxCells = 80;
inline constexpr int residualCoveredGuardRadius = 0;
inline constexpr double simplifyTolerance = 2.0;
inline constexpr int smoothIterations = 1;

template <typename Parameters>
void applyVisibleProductionDefaults(Parameters& p) {
    p.threshold = threshold;
    p.percentile = percentile;
    p.minArea = minArea;
    p.closeRadius = closeRadius;
    p.weakPercentile = weakPercentile;
    p.weakConnectRadius = weakConnectRadius;
    p.beltRadius = beltRadius;
    p.postBeltCloseRadius = postBeltCloseRadius;
    p.attributeConstrainedBelt = attributeConstrainedBelt;
    p.attributeEnvelopeRadius = attributeEnvelopeRadius;
    p.minBranchLength = minBranchLength;
    p.minJunctionBranchLength = minJunctionBranchLength;
    p.minJunctionBranchMean = minJunctionBranchMean;
    p.maxGapDistance = maxGapDistance;
    p.maxGapAngle = maxGapAngle;
    p.minGapSupportRatio = minGapSupportRatio;
    p.separateBranchBelts = separateBranchBelts;
    p.rootAntiMerge = rootAntiMerge;
    p.rootAntiMergeRadius = rootAntiMergeRadius;
    p.rootAntiMergeSupportRatio = rootAntiMergeSupportRatio;
    p.rootAntiMergeMaxBridgeValue = rootAntiMergeMaxBridgeValue;
    p.rootAntiMergeMinPartCells = rootAntiMergeMinPartCells;
    p.rootAntiMergeMinPartRatio = rootAntiMergeMinPartRatio;
    p.rootAntiMergeWeakGuardThreshold = rootAntiMergeWeakGuardThreshold;
    p.rootAntiMergeWeakGuardRadius = rootAntiMergeWeakGuardRadius;
    p.localAntiMerge = localAntiMerge;
    p.localAntiMergeLowThreshold = localAntiMergeLowThreshold;
    p.localAntiMergeBridgeRadius = localAntiMergeBridgeRadius;
    p.localAntiMergeMinPartCells = localAntiMergeMinPartCells;
    p.localAntiMergeMinPartRatio = localAntiMergeMinPartRatio;
    p.localAntiMergeMinBridgeCells = localAntiMergeMinBridgeCells;
    p.localAntiMergeMinLowRatio = localAntiMergeMinLowRatio;
    p.residualTrace = residualTrace;
    p.weakTailTopologyGate = weakTailTopologyGate;
    p.simpleRingCompatibility = simpleRingCompatibility;
    p.simpleRingMaximumFillCells = simpleRingMaximumFillCells;
    p.simpleRingMaximumOpeningCells = simpleRingMaximumOpeningCells;
    p.residualPercentile = residualPercentile;
    p.residualFillRadius = residualFillRadius;
    p.residualEdgeFillRadius = residualEdgeFillRadius;
    p.residualEdgeFillMinValueRatio = residualEdgeFillMinValueRatio;
    p.residualEdgeFillMaxPatchSpan = residualEdgeFillMaxPatchSpan;
    p.residualMinCells = residualMinCells;
    p.residualTraceMinLength = residualTraceMinLength;
    p.residualTraceMaxCells = residualTraceMaxCells;
    p.residualCoveredGuardRadius = residualCoveredGuardRadius;
    p.simplifyTolerance = simplifyTolerance;
    p.smoothIterations = smoothIterations;
}

} // namespace fpt::product_defaults
