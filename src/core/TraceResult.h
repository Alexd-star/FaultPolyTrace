#pragma once

#include "AdaptiveBoundary.h"
#include "BranchAttribution.h"
#include "LocalPolygonQuality.h"
#include "MultiScaleEvidence.h"
#include "Polygon.h"
#include "Segmentation.h"
#include "SimpleRingCompatibility.h"
#include "SkeletonGraphV2.h"
#include "SkeletonGraphV2Attribution.h"
#include "WeakContinuityConnector.h"
#include "WeakExtensionGate.h"
#include "WeakTailRestorer.h"

#include <vector>

namespace fpt {

struct BranchTrace {
    int id = 0;
    int pointCount = 0;
    double length = 0.0;
    double meanValue = 0.0;
    double maxValue = 0.0;
    bool kept = true;
    bool startIsJunction = false;
    bool endIsJunction = false;
    std::vector<Point> points;
};

struct TraceStageTimings {
    double validationMs = 0.0;
    double segmentationMs = 0.0;
    double preprocessingMs = 0.0;
    double ridgeTracingMs = 0.0;
    double beltAndAntiMergeMs = 0.0;
    double beltConstructionMs = 0.0;
    double rootAntiMergeMs = 0.0;
    double localAntiMergeMs = 0.0;
    double finalWeakContinuityMs = 0.0;
    double finalWeakGraphMs = 0.0;
    double finalWeakSearchMs = 0.0;
    double finalWeakGateMs = 0.0;
    double finalWeakGateContextMs = 0.0;
    double finalWeakGateOwnershipMs = 0.0;
    double finalWeakGateComponentHoleMs = 0.0;
    double finalWeakGateEnvelopeMs = 0.0;
    double finalWeakGateTopologyMs = 0.0;
    double simpleRingCompatibilityMs = 0.0;
    double vectorizationMs = 0.0;
    double postProcessingMs = 0.0;
    double totalMs = 0.0;
};

struct TraceResult {
    TraceStageTimings stageTimings;
    std::vector<Polygon> polygons;
    std::vector<BranchTrace> branches;
    // Segmentation/belt mask cells before optional polygon-only local repair.
    int activeCellCount = 0;
    bool physicalMorphologyShadowEvaluated = false;
    double physicalMorphologyRadiusWorld = 0.0;
    int physicalMorphologyLegacyCells = 0;
    int physicalMorphologyCandidateCells = 0;
    int physicalMorphologyAddedCells = 0;
    int physicalMorphologyRemovedCells = 0;
    double physicalMorphologyJaccard = 1.0;
    double effectiveThreshold = 0.0;
    PercentileThresholdInfo primaryThresholdInfo;
    PercentileThresholdInfo weakThresholdInfo;
    PercentileThresholdInfo residualThresholdInfo;
    int skeletonEndpoints = 0;
    int skeletonJunctions = 0;
    int skeletonBranches = 0;
    bool skeletonGraphV2ShadowEvaluated = false;
    SkeletonGraphV2Stats skeletonGraphV2Stats;
    bool skeletonGraphV2AttributionShadowEvaluated = false;
    SkeletonGraphV2AttributionComparison skeletonGraphV2Attribution;
    bool multiScaleEvidenceShadowEvaluated = false;
    MultiScaleEvidenceSummary multiScaleEvidenceSummary;
    bool adaptiveBoundaryShadowEvaluated = false;
    bool adaptiveBoundaryApplied = false;
    AdaptiveBoundarySummary adaptiveBoundarySummary;
    AdaptiveBoundaryCommitSummary adaptiveBoundaryCommitSummary;
    std::vector<AdaptiveBoundaryEvidence> adaptiveBoundaryEvidence;
    int keptBranches = 0;
    int prunedBranches = 0;
    int connectedGaps = 0;
    WeakContinuityStats weakContinuityStats;
    std::vector<WeakContinuityCandidate> weakContinuityCandidates;
    int weakContinuityProtectedFromAntiMergeCells = 0;
    int weakContinuityFinalObjectAcceptedConnections = 0;
    int weakContinuityFinalObjectRejectedConnections = 0;
    int weakContinuityFinalObjectAddedEnvelopeCells = 0;
    WeakTailStats weakTailStats;
    std::vector<WeakTailCandidate> weakTailCandidates;
    bool weakTailEndpointPortShadowEvaluated = false;
    WeakTailStats weakTailEndpointPortShadowStats;
    std::vector<WeakTailCandidate> weakTailEndpointPortCandidates;
    int weakTailProtectedFromAntiMergeCells = 0;
    bool simpleRingCompatibilityApplied = false;
    SimpleRingCompatibilityStats simpleRingCompatibilityStats;
    int rootAntiMergeCandidateCutCells = 0;
    int rootAntiMergeCutCells = 0;
    int rootAntiMergeRejectedCutCells = 0;
    int rootAntiMergeAcceptedComponents = 0;
    int rootAntiMergeRejectedComponents = 0;
    int rootAntiMergeWeakGuardRejectedCutCells = 0;
    int rootAntiMergeWeakGuardRejectedComponents = 0;
    int localAntiMergeCandidateCutCells = 0;
    int localAntiMergeCutCells = 0;
    int localAntiMergeRejectedCutCells = 0;
    int localAntiMergeAcceptedComponents = 0;
    int localAntiMergeRejectedComponents = 0;
    int residualFillCandidateCells = 0;
    int residualFillAddedCells = 0;
    int residualFillRejectedComponents = 0;
    double residualTraceEffectiveThreshold = 0.0;
    int residualTraceCandidateComponents = 0;
    int residualTraceSizeRejectedComponents = 0;
    int residualTraceShapeRejectedComponents = 0;
    int residualTraceAttributeRejectedComponents = 0;
    int residualTraceDetachedRejectedComponents = 0;
    int residualTraceGeometryRejectedComponents = 0;
    int residualTraceTopologyRejectedComponents = 0;
    int residualTraceFallbackAcceptedComponents = 0;
    int residualTraceAcceptedComponents = 0;
    int residualTraceAcceptedPolygons = 0;
    int residualTraceAcceptedCells = 0;
    int weakPathCandidateCells = 0;
    int weakPathAcceptedCells = 0;
    int weakPathRejectedCells = 0;
    int weakPathSeedCells = 0;
    int weakPathSeedContactCells = 0;
    int weakPathRootConflictRejectedCells = 0;
    int weakPathCandidateComponents = 0;
    int weakPathAcceptedComponents = 0;
    int weakPathRejectedComponents = 0;
    int weakPathPartiallyRejectedComponents = 0;
    int weakPathEvidenceEvaluatedFragments = 0;
    int weakPathAttributeRejectedFragments = 0;
    int weakPathShapeRejectedFragments = 0;
    int weakPathDirectionRejectedFragments = 0;
    bool weakExtensionLayeredShadowEvaluated = false;
    int weakExtensionComponentCount = 0;
    int weakExtensionLegacyAcceptedComponents = 0;
    int weakExtensionLayeredAcceptedComponents = 0;
    int weakExtensionLayeredRejectedComponents = 0;
    int weakExtensionLayeredOwnerRejectedComponents = 0;
    int weakExtensionLayeredSizeRejectedComponents = 0;
    int weakExtensionLayeredShapeRejectedComponents = 0;
    int weakExtensionLayeredSupportRejectedComponents = 0;
    int weakExtensionLayeredWidthRejectedComponents = 0;
    int weakExtensionLegacyOnlyCells = 0;
    int weakExtensionLayeredOnlyCells = 0;
    std::vector<WeakExtensionGateEvidence> weakExtensionLayeredEvidence;
    int branchAttributionJunctionClusters = 0;
    int branchAttributionAcceptedPairs = 0;
    int branchAttributionAmbiguousClusters = 0;
    int branchAttributionUnmatchedEnds = 0;
    int branchAttributionGreedyFallbackClusters = 0;
    int branchAttributionCandidatePairingDifferingClusters = 0;
    int branchAttributionCandidateDifferingClusters = 0;
    int branchAttributionAcceptedPairingDifferingClusters = 0;
    int branchAttributionAcceptedDifferingClusters = 0;
    int branchAttributionPairingDifferingClusters = 0;
    int branchAttributionDifferingClusters = 0;
    int branchAttributionCandidateGroupingAppliedClusters = 0;
    int branchAttributionCandidateGroupingAmbiguousFallbackClusters = 0;
    int branchAttributionCandidateGroupingLegacyFallbackClusters = 0;
    int branchAttributionCandidateGroupingComplexityFallbackClusters = 0;
    int branchAttributionCandidateGroupingSpanFallbackClusters = 0;
    int branchAttributionCandidateGroupingExcludedInternalBranches = 0;
    int branchAttributionCandidateGroupingRootGroups = 0;
    std::vector<JunctionAttributionResult> branchAttributionJunctions;
    LocalPolygonQualityStats localPolygonQualityStats;
    std::vector<LocalPolygonQualityEvidence> localPolygonQualityEvidence;
    int suppressedInternalRings = 0;
    int coveragePreserveSimplifyFallbackPolygons = 0;
    int coveragePreserveSmoothFallbackPolygons = 0;
    int coveragePreserveTopologyOverridePolygons = 0;
};


} // namespace fpt
