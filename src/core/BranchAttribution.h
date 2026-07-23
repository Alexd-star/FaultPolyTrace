#pragma once

#include "Grid2D.h"
#include "SkeletonGraph.h"

#include <limits>
#include <vector>

namespace fpt {

struct BranchDirection {
    double x = 1.0;
    double y = 0.0;
    bool valid = false;
};

struct BranchAttributionEnd {
    int branchId = -1;
    double directionX = 1.0;
    double directionY = 0.0;
    double meanSupport = 1.0;
    double localWidth = 1.0;
    double backgroundIsolation = 1.0;
};

struct BranchAttributionOptions {
    std::vector<int> directionWindows = {1, 3, 5};
    int junctionClusterRadius = 1;
    int attributeSampleCells = 8;
    int widthSampleRadius = 4;
    int maxExactEnds = 12;
    double supportThreshold = 0.0;
    double angleWeight = 0.65;
    double attributeWeight = 0.15;
    double widthWeight = 0.10;
    double backgroundWeight = 0.10;
    double maxTurnAngleDegrees = 75.0;
    double maxPairCost = 0.75;
    double unmatchedCost = 0.60;
    double ambiguityMargin = 0.05;
};

struct BranchAttributionPair {
    int firstBranchId = -1;
    int secondBranchId = -1;
    double cost = 0.0;
    double turnAngleDegrees = 0.0;
    double angleCost = 0.0;
    double attributeCost = 0.0;
    double widthCost = 0.0;
    double backgroundCost = 0.0;
};

struct JunctionAttributionResult {
    int clusterId = 0;
    int minGridX = 0;
    int minGridY = 0;
    int maxGridX = 0;
    int maxGridY = 0;
    double centerGridX = 0.0;
    double centerGridY = 0.0;
    double centerWorldX = 0.0;
    double centerWorldY = 0.0;
    std::vector<SkeletonPoint> junctionPoints;
    std::vector<BranchAttributionPair> pairs;
    std::vector<int> unmatchedBranchIds;
    std::vector<BranchAttributionPair> legacyPairs;
    std::vector<int> legacyUnmatchedBranchIds;
    double bestCost = 0.0;
    double alternativeCost = std::numeric_limits<double>::infinity();
    double confidence = 1.0;
    bool ambiguous = false;
    bool usedExactMatching = true;
    bool pairingDiffersFromLegacy = false;
    bool differsFromLegacy = false;
    bool acceptedPairingDiffersFromLegacy = false;
    bool acceptedDiffersFromLegacy = false;
};

struct BranchAttributionSummary {
    int junctionClusterCount = 0;
    int acceptedPairCount = 0;
    int ambiguousClusterCount = 0;
    int unmatchedEndCount = 0;
    int greedyFallbackClusterCount = 0;
    int candidatePairingDifferingClusterCount = 0;
    int candidateDifferingClusterCount = 0;
    int acceptedPairingDifferingClusterCount = 0;
    int acceptedDifferingClusterCount = 0;
    int pairingDifferingClusterCount = 0;
    int differingClusterCount = 0;
    std::vector<JunctionAttributionResult> junctions;
};

struct LegacyGreedyBranchPair {
    int firstBranchIndex = -1;
    int secondBranchIndex = -1;
    int firstBranchId = -1;
    int secondBranchId = -1;
    SkeletonPoint junction;
    double turnAngleDegrees = 0.0;
};

struct LegacyGreedyBranchPairingResult {
    std::vector<LegacyGreedyBranchPair> pairs;
    std::vector<std::vector<int>> rootBranchIndices;
};

struct CandidateRootGroupingResult {
    std::vector<std::vector<int>> rootBranchIndices;
    std::vector<int> excludedInternalBranchIndices;
    int appliedDifferingClusterCount = 0;
    int ambiguousFallbackClusterCount = 0;
    int legacyFallbackClusterCount = 0;
    int complexityFallbackClusterCount = 0;
    int spanFallbackClusterCount = 0;
};

struct CandidateRootGroupingOptions {
    double maxTurnAngleDegrees = 75.0;
    int maxJunctionSpanCells = 8;
    int maxInternalBranchCount = 4;
    double maxInternalToExternalRatio = 2.0;
};

LegacyGreedyBranchPairingResult computeLegacyGreedyBranchPairing(
    const std::vector<const SkeletonBranch*>& branches,
    double maxTurnAngleDegrees);

CandidateRootGroupingResult computeCandidateRootGrouping(
    const std::vector<const SkeletonBranch*>& branches,
    const std::vector<JunctionAttributionResult>& junctions,
    const CandidateRootGroupingOptions& options =
        CandidateRootGroupingOptions{});

BranchDirection estimateMultiScaleBranchDirection(
    const Grid2D& grid,
    const std::vector<SkeletonPoint>& pointsFromJunction,
    const std::vector<int>& windows);

JunctionAttributionResult matchBranchEnds(
    const std::vector<BranchAttributionEnd>& ends,
    const BranchAttributionOptions& options = BranchAttributionOptions{});

JunctionAttributionResult matchBranchEndsLegacyGreedy(
    const std::vector<BranchAttributionEnd>& ends,
    const BranchAttributionOptions& options = BranchAttributionOptions{});

BranchAttributionSummary analyzeBranchAttribution(
    const Grid2D& grid,
    const std::vector<const SkeletonBranch*>& branches,
    const BranchAttributionOptions& options = BranchAttributionOptions{});

} // namespace fpt
