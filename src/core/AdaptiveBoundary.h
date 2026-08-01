#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "MultiScaleEvidence.h"
#include "Polygon.h"

#include <cstddef>
#include <string>
#include <vector>

namespace fpt {

enum class AdaptiveBoundaryDecision {
    NoChange,
    AcceptedCandidate,
    Review,
    Rejected
};

struct AdaptiveBoundaryOptions {
    double maximumDisplacementWorld = 15.0;
    double searchStepWorld = 0.0;
    double supportThreshold = 0.0;
    double minimumSupportGain = 0.01;
    double maximumSupportLossRatio = 0.0;
    double maximumBackgroundLeakageIncreaseRatio = 0.0;
    double maximumAreaChangeRatio = 0.08;
    double offsetRegularization = 0.35;
    int localRadiusCells = 1;
    std::size_t maximumVerticesPerPolygon = 4096;
    std::size_t maximumPolygons = 512;
    std::size_t maximumRasterCellsPerPolygon = 200000;
};

struct AdaptiveBoundaryEvidence {
    int polygonId = 0;
    AdaptiveBoundaryDecision decision = AdaptiveBoundaryDecision::NoChange;
    int movedVertices = 0;
    double maximumDisplacementWorld = 0.0;
    double originalArea = 0.0;
    double candidateArea = 0.0;
    double supportGainRatio = 0.0;
    double boundaryScoreGain = 0.0;
    double supportLossRatio = 0.0;
    double backgroundLeakageIncreaseRatio = 0.0;
    double areaChangeRatio = 0.0;
    std::size_t rasterCellsEvaluated = 0;
    bool candidateFinite = true;
    bool candidateChanged = false;
    bool topologyValid = true;
    bool rasterBudgetExceeded = false;
    std::string reason;
    Polygon candidate;
};

struct AdaptiveBoundarySummary {
    int polygonsEvaluated = 0;
    int noChange = 0;
    int acceptedCandidates = 0;
    int reviewCandidates = 0;
    int rejectedCandidates = 0;
    int topologyRejectedCandidates = 0;
    int rasterBudgetReviewCandidates = 0;
    int movedVertices = 0;
    int truncatedPolygons = 0;
    std::size_t rasterCellsEvaluated = 0;
    double maximumDisplacementWorld = 0.0;
    double meanSupportGainRatio = 0.0;
    double meanBoundaryScoreGain = 0.0;
    double meanBackgroundLeakageIncreaseRatio = 0.0;
};

struct AdaptiveBoundaryResult {
    AdaptiveBoundarySummary summary;
    std::vector<AdaptiveBoundaryEvidence> evidence;
};

void validateAdaptiveBoundaryOptions(const AdaptiveBoundaryOptions& options);

AdaptiveBoundaryResult evaluateAdaptiveBoundaries(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const std::vector<Polygon>& polygons,
    const MultiScaleEvidenceResult* auxiliaryEvidence,
    const AdaptiveBoundaryOptions& options);

inline const char* adaptiveBoundaryDecisionName(AdaptiveBoundaryDecision decision) {
    switch (decision) {
    case AdaptiveBoundaryDecision::NoChange:
        return "no_change";
    case AdaptiveBoundaryDecision::AcceptedCandidate:
        return "accepted_candidate";
    case AdaptiveBoundaryDecision::Review:
        return "review";
    case AdaptiveBoundaryDecision::Rejected:
        return "rejected";
    }
    return "unknown";
}

} // namespace fpt
