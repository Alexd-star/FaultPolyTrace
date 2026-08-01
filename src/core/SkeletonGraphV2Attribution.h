#pragma once

#include "BranchAttribution.h"
#include "SkeletonGraphV2.h"

namespace fpt {

struct SkeletonGraphV2AttributionComparison {
    bool evaluated = false;
    int legacyBranchCount = 0;
    int v2BranchCount = 0;
    int convertedBranchCount = 0;
    int skippedClosedLoopBranches = 0;
    int invalidBranches = 0;
    int legacyJunctionClusters = 0;
    int v2JunctionClusters = 0;
    int legacyAcceptedPairs = 0;
    int v2AcceptedPairs = 0;
    int legacyAmbiguousClusters = 0;
    int v2AmbiguousClusters = 0;
    int legacyUnmatchedEnds = 0;
    int v2UnmatchedEnds = 0;
    int acceptedPairCountDelta = 0;
    int ambiguousClusterCountDelta = 0;
    int unmatchedEndCountDelta = 0;
    int branchCountDelta = 0;
    int junctionClusterCountDelta = 0;
    int matchedJunctionClusters = 0;
    int equivalentMatchedJunctionClusters = 0;
    int differingMatchedJunctionClusters = 0;
    int legacyOnlyJunctionClusters = 0;
    int v2OnlyJunctionClusters = 0;
    int ambiguityImprovedJunctionClusters = 0;
    int ambiguityRegressedJunctionClusters = 0;
    int v2AmbiguousPortAttachments = 0;
    double meanMatchedCenterDistanceCells = 0.0;
    double maximumMatchedCenterDistanceCells = 0.0;
    BranchAttributionSummary legacy;
    BranchAttributionSummary v2;
};

SkeletonGraphV2AttributionComparison compareSkeletonGraphV2Attribution(
    const Grid2D& grid,
    const std::vector<const SkeletonBranch*>& legacyBranches,
    const SkeletonGraphV2& graphV2,
    const BranchAttributionOptions& options = BranchAttributionOptions{});

} // namespace fpt
