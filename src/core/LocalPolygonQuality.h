#pragma once

#include "Grid2D.h"
#include "Polygon.h"

#include <cstddef>
#include <vector>

namespace fpt {

enum class LocalQualityCandidateType {
    LocalBulge,
    LowSupportBridge,
    LegalWideBelt,
    NaturalEndCap,
    ObjectBudgetExceeded,
    InvalidGeometry
};

enum class LocalQualityDecision {
    Accepted,
    Review,
    Rejected
};

enum class LocalQualityRejectionReason {
    None,
    InsufficientEvidence,
    NoMeaningfulChange,
    ExcessiveAreaLoss,
    StrongCoverageLoss,
    WeakCoverageLoss,
    BackgroundLeakage,
    TopologyRisk,
    StabilityRisk,
    InvalidGeometry
};

struct LocalPolygonQualityOptions {
    double widthRatio = 2.4;
    double referenceWidthCells = 0.0;
    int minExtraCells = 8;
    int candidatePaddingCells = 1;
    double supportThresholdRatio = 0.70;
    int supportBufferCells = 1;
    int minSupportCells = 4;
    double bridgeMaxSupportRatio = 0.25;
    int bridgeMinCells = 2;
    double maxAreaLossRatio = 0.18;
    double maxStrongCoverageLossRatio = 0.0;
    double maxWeakCoverageLossRatio = 0.0;
    double maxBackgroundLeakageIncreaseRatio = 0.0;
    double maxLocalizedSpanRatio = 0.45;
    double endpointGuardRatio = 0.15;
    double stabilityThresholdDeltaRatio = 0.02;
    double minStabilityJaccard = 0.90;
    std::size_t maxCandidatesPerObject = 64;
    std::size_t maxTotalCandidates = 4096;
    std::size_t maxWindowCells = 65536;
    std::size_t maxObjectCells = 2000000;
    std::size_t maxEvidenceRecords = 4096;
};

struct LocalPolygonQualityEvidence {
    int objectIndex = 0;
    int polygonId = 0;
    LocalQualityCandidateType type = LocalQualityCandidateType::LocalBulge;
    LocalQualityDecision decision = LocalQualityDecision::Review;
    LocalQualityRejectionReason rejectionReason = LocalQualityRejectionReason::None;
    int minGridX = 0;
    int maxGridX = 0;
    int minGridY = 0;
    int maxGridY = 0;
    int candidateCellCount = 0;
    int supportCellCount = 0;
    int removedCellCount = 0;
    double baselineWidth = 0.0;
    double localWidth = 0.0;
    double widthRatio = 0.0;
    double spanRatio = 0.0;
    double supportRatio = 0.0;
    double areaLossRatio = 0.0;
    double strongCoverageLossRatio = 0.0;
    double weakCoverageLossRatio = 0.0;
    double backgroundLeakageDelta = 0.0;
    double stabilityJaccard = 0.0;
};

struct LocalPolygonQualityStats {
    int detectedCount = 0;
    int acceptedCount = 0;
    int reviewCount = 0;
    int rejectedCount = 0;
    int insufficientEvidenceRejectedCount = 0;
    int noMeaningfulChangeRejectedCount = 0;
    int areaLossRejectedCount = 0;
    int strongCoverageRejectedCount = 0;
    int weakCoverageRejectedCount = 0;
    int backgroundLeakageRejectedCount = 0;
    int topologyRejectedCount = 0;
    int stabilityRejectedCount = 0;
    int invalidGeometryRejectedCount = 0;
    int candidateBudgetReviewCount = 0;
    int windowBudgetReviewCount = 0;
    int objectBudgetReviewCount = 0;
    int postRepairFilteredCount = 0;
    std::size_t totalEvidence = 0;
    std::size_t storedEvidence = 0;
    std::size_t truncatedEvidence = 0;
};

struct LocalPolygonQualityResult {
    std::vector<Polygon> polygons;
    std::vector<LocalPolygonQualityEvidence> evidence;
    LocalPolygonQualityStats stats;
};

void validateLocalPolygonQualityOptions(
    const LocalPolygonQualityOptions& options);

LocalPolygonQualityResult applyLocalPolygonQuality(
    const Grid2D& grid,
    const std::vector<Polygon>& polygons,
    double effectiveThreshold,
    const LocalPolygonQualityOptions& options);

const char* localQualityCandidateTypeName(LocalQualityCandidateType type);
const char* localQualityDecisionName(LocalQualityDecision decision);
const char* localQualityRejectionReasonName(LocalQualityRejectionReason reason);

} // namespace fpt
