#include "../src/core/BranchAttribution.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(
    int width = 20,
    int height = 20,
    double dx = 1.0,
    double dy = 1.0,
    double value = 1.0,
    double sx = 0.0,
    double sy = 0.0) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    metadata.sx = sx;
    metadata.sy = sy;
    metadata.dx = dx;
    metadata.dy = dy;
    fpt::Grid2D grid(metadata);
    for (double& cell : grid.values()) {
        cell = value;
    }
    return grid;
}

fpt::BranchAttributionEnd makeEnd(
    int branchId,
    double angleDegrees,
    double meanSupport = 0.95,
    double localWidth = 1.0,
    double backgroundIsolation = 0.95) {
    const double radians =
        angleDegrees * 3.14159265358979323846 / 180.0;
    fpt::BranchAttributionEnd end;
    end.branchId = branchId;
    end.directionX = std::cos(radians);
    end.directionY = std::sin(radians);
    end.meanSupport = meanSupport;
    end.localWidth = localWidth;
    end.backgroundIsolation = backgroundIsolation;
    return end;
}

std::vector<std::pair<int, int>> normalizedPairs(
    const fpt::JunctionAttributionResult& result) {
    std::vector<std::pair<int, int>> pairs;
    for (const fpt::BranchAttributionPair& pair : result.pairs) {
        pairs.push_back({
            std::min(pair.firstBranchId, pair.secondBranchId),
            std::max(pair.firstBranchId, pair.secondBranchId)});
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

std::vector<std::pair<int, int>> normalizedLegacyPairs(
    const fpt::JunctionAttributionResult& result) {
    std::vector<std::pair<int, int>> pairs;
    for (const fpt::BranchAttributionPair& pair : result.legacyPairs) {
        pairs.push_back({
            std::min(pair.firstBranchId, pair.secondBranchId),
            std::max(pair.firstBranchId, pair.secondBranchId)});
    }
    std::sort(pairs.begin(), pairs.end());
    return pairs;
}

std::vector<std::vector<int>> normalizedRootPartitions(
    const std::vector<const fpt::SkeletonBranch*>& branches,
    const fpt::LegacyGreedyBranchPairingResult& result) {
    std::vector<std::vector<int>> partitions;
    for (const std::vector<int>& group : result.rootBranchIndices) {
        std::vector<int> ids;
        for (const int branchIndex : group) {
            ids.push_back(
                branches[static_cast<std::size_t>(branchIndex)]->id);
        }
        std::sort(ids.begin(), ids.end());
        partitions.push_back(std::move(ids));
    }
    std::sort(partitions.begin(), partitions.end());
    return partitions;
}

std::vector<std::vector<int>> normalizedCandidateRootPartitions(
    const std::vector<const fpt::SkeletonBranch*>& branches,
    const fpt::CandidateRootGroupingResult& result) {
    std::vector<std::vector<int>> partitions;
    for (const std::vector<int>& group : result.rootBranchIndices) {
        std::vector<int> ids;
        for (const int branchIndex : group) {
            ids.push_back(
                branches[static_cast<std::size_t>(branchIndex)]->id);
        }
        std::sort(ids.begin(), ids.end());
        partitions.push_back(std::move(ids));
    }
    std::sort(partitions.begin(), partitions.end());
    return partitions;
}

std::vector<int> candidateExcludedBranchIds(
    const std::vector<const fpt::SkeletonBranch*>& branches,
    const fpt::CandidateRootGroupingResult& result) {
    std::vector<int> ids;
    for (const int branchIndex : result.excludedInternalBranchIndices) {
        ids.push_back(
            branches[static_cast<std::size_t>(branchIndex)]->id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void testStraightContinuationIsPaired() {
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 180.0),
            makeEnd(2, 0.0)});

    require(normalizedPairs(result) == std::vector<std::pair<int, int>>{{1, 2}},
            "Opposing straight branches must be paired.");
    require(!result.ambiguous, "A unique straight continuation is not ambiguous.");
}

void testXJunctionUsesOpposingPairs() {
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 0.0),
            makeEnd(2, 90.0),
            makeEnd(3, 180.0),
            makeEnd(4, 270.0)});

    require(
        normalizedPairs(result) ==
            std::vector<std::pair<int, int>>{{1, 3}, {2, 4}},
        "An X junction must pair the two opposing continuations.");
    require(result.unmatchedBranchIds.empty(), "All four X-junction arms should be matched.");
}

void testTJunctionLeavesSideBranchUnmatched() {
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 180.0),
            makeEnd(2, 0.0),
            makeEnd(3, 90.0)});

    require(normalizedPairs(result) == std::vector<std::pair<int, int>>{{1, 2}},
            "The T-junction crossbar must remain the primary continuation.");
    require(
        result.unmatchedBranchIds == std::vector<int>{3},
        "The T-junction side branch must remain unmatched.");
}

void testNonSymmetricYSelectsBestContinuation() {
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 180.0),
            makeEnd(2, 8.0),
            makeEnd(3, 100.0)});

    require(normalizedPairs(result) == std::vector<std::pair<int, int>>{{1, 2}},
            "A non-symmetric Y junction must select the most continuous pair.");
    require(!result.ambiguous, "A clearly non-symmetric Y junction is not ambiguous.");
}

void testSymmetricYIsReportedAsAmbiguous() {
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 0.0),
            makeEnd(2, 120.0),
            makeEnd(3, 240.0)});

    require(result.ambiguous, "A symmetric Y junction must be reported as ambiguous.");
    require(result.confidence <= 0.05, "Symmetric alternatives must have low confidence.");
}

void testGlobalMatchingBeatsInputOrderGreedyChoice() {
    fpt::BranchAttributionOptions options;
    options.angleWeight = 1.0;
    options.attributeWeight = 0.0;
    options.widthWeight = 0.0;
    options.backgroundWeight = 0.0;
    options.maxTurnAngleDegrees = 180.0;
    options.maxPairCost = 1.0;
    options.unmatchedCost = 2.0;
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 0.0),
            makeEnd(2, 10.0),
            makeEnd(3, 20.0),
            makeEnd(4, 30.0),
            makeEnd(5, 40.0),
            makeEnd(6, 200.0)},
            options);

    require(
        normalizedPairs(result) ==
            std::vector<std::pair<int, int>>{{1, 4}, {2, 5}, {3, 6}},
        "The matcher must minimize total junction cost instead of making a local greedy choice.");
}

void testLowSupportShortBranchCannotStealMainContinuation() {
    const fpt::JunctionAttributionResult result =
        fpt::matchBranchEnds({
            makeEnd(1, 180.0, 0.95),
            makeEnd(2, 10.0, 0.95),
            makeEnd(3, 0.0, 0.05)});

    require(
        normalizedPairs(result) == std::vector<std::pair<int, int>>{{1, 2}},
        "A low-support short branch must not steal the main continuation only because its angle is perfect.");
}

void testInputPermutationDoesNotChangeMatching() {
    const std::vector<fpt::BranchAttributionEnd> ordered = {
        makeEnd(1, 0.0),
        makeEnd(2, 90.0),
        makeEnd(3, 180.0),
        makeEnd(4, 270.0)};
    std::vector<fpt::BranchAttributionEnd> permuted = ordered;
    std::reverse(permuted.begin(), permuted.end());

    require(
        normalizedPairs(fpt::matchBranchEnds(ordered)) ==
            normalizedPairs(fpt::matchBranchEnds(permuted)),
        "Junction matching must be invariant to input branch order.");
}

void testDirectionUsesMultipleWindowsAndPhysicalSpacing() {
    const fpt::Grid2D grid = makeGrid(20, 20, 10.0, 1.0);
    const fpt::BranchDirection direction =
        fpt::estimateMultiScaleBranchDirection(
            grid,
            {{2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 6}},
            {1, 2, 4});

    require(direction.valid, "A multi-point branch must yield a valid direction.");
    require(std::abs(direction.x) > 0.99,
            "Direction estimation must use physical dx/dy and multiple windows.");
    require(std::abs(direction.y) < 0.15,
            "An anisotropic diagonal in cell space is physically near-horizontal.");
}

void testAdjacentJunctionPixelsAreAnalyzedAsOneCluster() {
    fpt::Grid2D grid = makeGrid();
    fpt::SkeletonBranch left;
    left.id = 1;
    left.startIsJunction = true;
    left.points = {{5, 5}, {4, 5}, {3, 5}, {2, 5}};
    fpt::SkeletonBranch right;
    right.id = 2;
    right.startIsJunction = true;
    right.points = {{6, 5}, {7, 5}, {8, 5}, {9, 5}};
    fpt::SkeletonBranch internalConnector;
    internalConnector.id = 3;
    internalConnector.startIsJunction = true;
    internalConnector.endIsJunction = true;
    internalConnector.points = {{5, 5}, {6, 5}};
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &left,
        &right,
        &internalConnector};

    fpt::BranchAttributionOptions options;
    options.junctionClusterRadius = 1;
    options.supportThreshold = 0.5;
    const fpt::BranchAttributionSummary summary =
        fpt::analyzeBranchAttribution(grid, branches, options);

    require(summary.junctionClusterCount == 1,
            "Adjacent junction pixels must be treated as one junction cluster.");
    require(summary.acceptedPairCount == 1,
            "The clustered straight branches must form one continuation pair while the internal connector is ignored.");
}

void testJunctionWorldCenterUsesCellCenterCoordinates() {
    fpt::Grid2D grid =
        makeGrid(20, 20, 2.5, 4.0, 1.0, 100.0, -50.0);
    fpt::SkeletonBranch left;
    left.id = 1;
    left.startIsJunction = true;
    left.points = {{2, 3}, {1, 3}, {0, 3}};
    fpt::SkeletonBranch right;
    right.id = 2;
    right.startIsJunction = true;
    right.points = {{2, 3}, {3, 3}, {4, 3}};

    fpt::BranchAttributionOptions options;
    options.supportThreshold = 0.5;
    const fpt::BranchAttributionSummary summary =
        fpt::analyzeBranchAttribution(
            grid,
            {&left, &right},
            options);

    require(summary.junctions.size() == 1,
            "The synthetic junction must produce one center.");
    const fpt::JunctionAttributionResult& junction =
        summary.junctions.front();
    require(std::abs(junction.centerGridX - 2.0) < 1e-12 &&
                std::abs(junction.centerGridY - 3.0) < 1e-12,
            "Grid-space center remains the mean junction cell index.");
    require(std::abs(junction.centerWorldX - 106.25) < 1e-12,
            "World X must map the junction cell center using centerGridX + 0.5.");
    require(std::abs(junction.centerWorldY + 36.0) < 1e-12,
            "World Y must map the junction cell center using centerGridY + 0.5.");
}

void testLegacyGreedyComparisonRetainsJunctionEvidence() {
    fpt::Grid2D grid = makeGrid();
    fpt::SkeletonBranch a;
    a.id = 1;
    a.startIsJunction = true;
    a.points = {{10, 10}, {11, 10}, {12, 10}, {13, 10}};
    fpt::SkeletonBranch b;
    b.id = 2;
    b.startIsJunction = true;
    b.points = {{10, 10}, {9, 10}, {8, 10}, {7, 10}};
    fpt::SkeletonBranch c;
    c.id = 3;
    c.startIsJunction = true;
    c.points = {{10, 10}, {10, 11}, {10, 12}, {10, 13}};
    const std::vector<const fpt::SkeletonBranch*> branches = {&a, &b, &c};

    fpt::BranchAttributionOptions options;
    options.supportThreshold = 0.5;
    options.maxTurnAngleDegrees = 100.0;
    const fpt::BranchAttributionSummary summary =
        fpt::analyzeBranchAttribution(grid, branches, options);

    require(summary.junctions.size() == 1,
            "The synthetic T junction must produce one detailed comparison.");
    const fpt::JunctionAttributionResult& junction = summary.junctions.front();
    require(normalizedLegacyPairs(junction) ==
                std::vector<std::pair<int, int>>{{1, 2}},
            "The comparison must reproduce the formal one-pixel greedy pair.");
    require(normalizedPairs(junction) ==
                std::vector<std::pair<int, int>>{{1, 2}},
            "The multi-scale matcher should agree on the clear T-junction trunk.");
    require(!junction.differsFromLegacy,
            "An agreeing junction must not be reported as a difference.");
    require(junction.minGridX == 10 && junction.maxGridX == 10 &&
                junction.minGridY == 10 && junction.maxGridY == 10,
            "The detailed comparison must retain the junction grid bounds.");
}

void testAmbiguousDifferenceIsCandidateEvidenceNotAcceptedDifference() {
    fpt::Grid2D grid = makeGrid();
    fpt::SkeletonBranch east;
    east.id = 1;
    east.startIsJunction = true;
    east.points = {{10, 10}, {11, 10}, {12, 10}};
    fpt::SkeletonBranch westFirst;
    westFirst.id = 2;
    westFirst.startIsJunction = true;
    westFirst.points = {{10, 10}, {9, 10}, {8, 10}};
    fpt::SkeletonBranch westLater;
    westLater.id = 3;
    westLater.startIsJunction = true;
    westLater.points = {{10, 10}, {9, 10}, {8, 10}};

    fpt::BranchAttributionOptions options;
    options.supportThreshold = 0.5;
    const fpt::BranchAttributionSummary summary =
        fpt::analyzeBranchAttribution(
            grid,
            {&east, &westFirst, &westLater},
            options);

    require(summary.junctions.size() == 1,
            "The tied junction must produce one auditable result.");
    const fpt::JunctionAttributionResult& junction =
        summary.junctions.front();
    require(junction.ambiguous,
            "Equal-cost pairing alternatives must remain ambiguous.");
    require(junction.pairingDiffersFromLegacy &&
                junction.differsFromLegacy,
            "The arbitrary tie-break must remain visible as candidate evidence.");
    require(!junction.acceptedPairingDiffersFromLegacy &&
                !junction.acceptedDiffersFromLegacy,
            "An ambiguous tie-break must never be marked as an accepted difference.");
    require(summary.candidatePairingDifferingClusterCount == 1 &&
                summary.candidateDifferingClusterCount == 1,
            "Candidate summary counts must retain ambiguous raw differences.");
    require(summary.acceptedPairingDifferingClusterCount == 0 &&
                summary.acceptedDifferingClusterCount == 0,
            "Accepted summary counts must exclude ambiguous differences.");
    require(summary.pairingDifferingClusterCount == 1 &&
                summary.differingClusterCount == 1,
            "Legacy unqualified counters remain candidate aliases for backward compatibility.");
}

void testAdjacentJunctionConnectorProducesAuditableLegacyDifference() {
    fpt::Grid2D grid = makeGrid();
    fpt::SkeletonBranch left;
    left.id = 1;
    left.startIsJunction = true;
    left.points = {{5, 5}, {4, 5}, {3, 5}, {2, 5}};
    fpt::SkeletonBranch right;
    right.id = 2;
    right.startIsJunction = true;
    right.points = {{6, 5}, {7, 5}, {8, 5}, {9, 5}};
    fpt::SkeletonBranch internalConnector;
    internalConnector.id = 3;
    internalConnector.startIsJunction = true;
    internalConnector.endIsJunction = true;
    internalConnector.points = {{5, 5}, {6, 5}};
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &left,
        &right,
        &internalConnector};

    fpt::BranchAttributionOptions options;
    options.junctionClusterRadius = 1;
    options.supportThreshold = 0.5;
    const fpt::BranchAttributionSummary summary =
        fpt::analyzeBranchAttribution(grid, branches, options);

    require(summary.pairingDifferingClusterCount == 1,
            "The shadow summary must retain the direct-pair representation difference.");
    require(summary.differingClusterCount == 0,
            "Connector-mediated legacy edges and the external new pair have the same root connectivity.");
    const fpt::JunctionAttributionResult& junction = summary.junctions.front();
    require(junction.pairingDiffersFromLegacy,
            "The internal connector must expose the direct-pair representation difference.");
    require(!junction.differsFromLegacy,
            "Equivalent external root connectivity must not be reported as an attribution conflict.");
    require(normalizedLegacyPairs(junction) ==
                std::vector<std::pair<int, int>>{{1, 3}, {2, 3}},
            "Legacy comparison must reproduce the connector-mediated transitive grouping.");
    require(normalizedPairs(junction) ==
                std::vector<std::pair<int, int>>{{1, 2}},
            "The clustered matcher must pair the two external continuations.");
    require(junction.minGridX == 5 && junction.maxGridX == 6,
            "Cluster bounds must cover both adjacent junction pixels.");
    require(!junction.pairs.empty() &&
                junction.pairs.front().angleCost >= 0.0 &&
                junction.pairs.front().attributeCost >= 0.0 &&
                junction.pairs.front().widthCost >= 0.0 &&
                junction.pairs.front().backgroundCost >= 0.0,
            "Accepted new pairs must expose all auditable cost components.");
}

void testSharedLegacyKernelPreservesLaterEqualAngleCandidateAndInputOrder() {
    fpt::SkeletonBranch east;
    east.id = 1;
    east.startIsJunction = true;
    east.points = {{10, 10}, {11, 10}, {12, 10}};
    fpt::SkeletonBranch westFirst;
    westFirst.id = 2;
    westFirst.startIsJunction = true;
    westFirst.points = {{10, 10}, {9, 10}, {8, 10}};
    fpt::SkeletonBranch westLater;
    westLater.id = 3;
    westLater.startIsJunction = true;
    westLater.points = {{10, 10}, {9, 10}, {8, 10}};

    const std::vector<const fpt::SkeletonBranch*> ordered = {
        &east, &westFirst, &westLater};
    const fpt::LegacyGreedyBranchPairingResult orderedResult =
        fpt::computeLegacyGreedyBranchPairing(ordered, 75.0);
    require(
        normalizedRootPartitions(ordered, orderedResult) ==
            std::vector<std::vector<int>>{{1, 3}, {2}},
        "The shared legacy kernel must preserve <= tie replacement by the later candidate.");

    const std::vector<const fpt::SkeletonBranch*> permuted = {
        &east, &westLater, &westFirst};
    const fpt::LegacyGreedyBranchPairingResult permutedResult =
        fpt::computeLegacyGreedyBranchPairing(permuted, 75.0);
    require(
        normalizedRootPartitions(permuted, permutedResult) ==
            std::vector<std::vector<int>>{{1, 2}, {3}},
        "The shared legacy kernel must preserve the formal input-order behavior.");
}

void testSharedLegacyKernelPreservesConnectorMediatedRootPartition() {
    fpt::SkeletonBranch left;
    left.id = 1;
    left.startIsJunction = true;
    left.points = {{5, 5}, {4, 5}, {3, 5}};
    fpt::SkeletonBranch right;
    right.id = 2;
    right.startIsJunction = true;
    right.points = {{6, 5}, {7, 5}, {8, 5}};
    fpt::SkeletonBranch internalConnector;
    internalConnector.id = 3;
    internalConnector.startIsJunction = true;
    internalConnector.endIsJunction = true;
    internalConnector.points = {{5, 5}, {6, 5}};
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &left, &right, &internalConnector};

    const fpt::LegacyGreedyBranchPairingResult result =
        fpt::computeLegacyGreedyBranchPairing(branches, 75.0);

    require(
        normalizedRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 2, 3}},
        "Two junction pixels connected by a double-ended internal branch must remain one formal root.");
    require(result.pairs.size() == 2,
            "The double-ended connector must participate in one legacy pair at each junction pixel.");
}

void testLegacyGreedyAndGlobalMatchingCanProduceDifferentRootPartitions() {
    fpt::BranchAttributionOptions options;
    options.angleWeight = 1.0;
    options.attributeWeight = 0.0;
    options.widthWeight = 0.0;
    options.backgroundWeight = 0.0;
    options.maxTurnAngleDegrees = 180.0;
    options.maxPairCost = 1.0;
    options.unmatchedCost = 2.0;
    const std::vector<fpt::BranchAttributionEnd> ends = {
        makeEnd(1, 0.0),
        makeEnd(2, 10.0),
        makeEnd(3, 20.0),
        makeEnd(4, 30.0),
        makeEnd(5, 40.0),
        makeEnd(6, 200.0)};

    const fpt::JunctionAttributionResult modern =
        fpt::matchBranchEnds(ends, options);
    const fpt::JunctionAttributionResult legacy =
        fpt::matchBranchEndsLegacyGreedy(ends, options);

    require(normalizedPairs(modern) != normalizedPairs(legacy),
            "The legacy one-pass greedy matcher must remain independently reproducible for comparison.");
    require(
        normalizedPairs(modern) ==
            std::vector<std::pair<int, int>>{{1, 4}, {2, 5}, {3, 6}},
        "The global matcher must retain the minimum-total-cost partition.");
}

void testMatcherRejectsUnsafeOptions() {
    const std::vector<fpt::BranchAttributionEnd> ends = {
        makeEnd(1, 0.0),
        makeEnd(2, 180.0)};

    auto requireRejected = [&](fpt::BranchAttributionOptions options,
                               const std::string& label) {
        bool rejected = false;
        try {
            (void)fpt::matchBranchEnds(ends, options);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, label);
    };

    fpt::BranchAttributionOptions negativeWeight;
    negativeWeight.angleWeight = -0.1;
    requireRejected(
        negativeWeight,
        "Negative evidence weights must be rejected.");

    fpt::BranchAttributionOptions nonFiniteCost;
    nonFiniteCost.maxPairCost =
        std::numeric_limits<double>::quiet_NaN();
    requireRejected(
        nonFiniteCost,
        "Non-finite matching costs must be rejected.");

    fpt::BranchAttributionOptions excessiveExactMatching;
    excessiveExactMatching.maxExactEnds = 13;
    requireRejected(
        excessiveExactMatching,
        "Exact matching above the bounded production limit must be rejected.");
}

void testCandidateRootGroupingAppliesOnlyAcceptedConnectivityDifference() {
    fpt::SkeletonBranch first;
    first.id = 1;
    fpt::SkeletonBranch second;
    second.id = 2;
    fpt::SkeletonBranch third;
    third.id = 3;
    fpt::SkeletonBranch internal;
    internal.id = 9;
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &first, &second, &third, &internal};

    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {
        {1, 9}, {9, 3}};
    junction.legacyUnmatchedBranchIds = {2};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = true;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(
            branches,
            {junction});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 2}, {3}},
        "An accepted root-connectivity difference must apply the modern external pairing.");
    require(
        candidateExcludedBranchIds(branches, result) ==
            std::vector<int>{9},
        "A connector used only inside the changed junction cluster must not become an ownership root.");
    require(result.appliedDifferingClusterCount == 1,
            "The accepted changed cluster must be counted as applied.");
}

void testCandidateRootGroupingFallsBackForAmbiguousDifference() {
    fpt::SkeletonBranch first;
    first.id = 1;
    fpt::SkeletonBranch second;
    second.id = 2;
    fpt::SkeletonBranch third;
    third.id = 3;
    fpt::SkeletonBranch internal;
    internal.id = 9;
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &first, &second, &third, &internal};

    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {
        {1, 9}, {9, 3}};
    junction.legacyUnmatchedBranchIds = {2};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.ambiguous = true;
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = false;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(
            branches,
            {junction});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 3, 9}, {2}},
        "An ambiguous junction must preserve the formal legacy root partition.");
    require(result.excludedInternalBranchIndices.empty(),
            "Ambiguous fallback must not discard formal connector branches.");
    require(result.ambiguousFallbackClusterCount == 1,
            "Ambiguous fallback must be auditable.");
}

void testCandidateRootGroupingFallsBackForNonExactMatching() {
    fpt::SkeletonBranch first;
    first.id = 1;
    fpt::SkeletonBranch second;
    second.id = 2;
    fpt::SkeletonBranch third;
    third.id = 3;
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &first, &second, &third};

    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {{1, 3}};
    junction.legacyUnmatchedBranchIds = {2};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.usedExactMatching = false;
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = true;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(branches, {junction});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 3}, {2}},
        "A non-exact greedy fallback must preserve the formal legacy root partition.");
    require(result.appliedDifferingClusterCount == 0,
            "A non-exact fallback must not enter candidate ownership.");
    require(result.legacyFallbackClusterCount == 1,
            "A non-exact fallback must remain auditable as a legacy fallback.");
}

void testCandidateRootGroupingKeepsLegacyForRepresentationOnlyDifference() {
    fpt::SkeletonBranch first;
    first.id = 1;
    fpt::SkeletonBranch second;
    second.id = 2;
    fpt::SkeletonBranch internal;
    internal.id = 9;
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &first, &second, &internal};

    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {
        {1, 9}, {9, 2}};
    junction.pairs = {{1, 2}};
    junction.pairingDiffersFromLegacy = true;
    junction.acceptedPairingDiffersFromLegacy = true;
    junction.differsFromLegacy = false;
    junction.acceptedDiffersFromLegacy = false;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(
            branches,
            {junction});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 2, 9}},
        "A direct-pair representation difference with identical root connectivity must keep legacy grouping.");
    require(result.appliedDifferingClusterCount == 0,
            "Representation-only differences must not alter ownership.");
}

void testCandidateRootGroupingIsInvariantToBranchVectorOrder() {
    fpt::SkeletonBranch first;
    first.id = 1;
    fpt::SkeletonBranch second;
    second.id = 2;
    fpt::SkeletonBranch third;
    third.id = 3;
    fpt::SkeletonBranch internal;
    internal.id = 9;
    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {
        {1, 9}, {9, 3}};
    junction.legacyUnmatchedBranchIds = {2};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = true;

    const std::vector<const fpt::SkeletonBranch*> ordered = {
        &first, &second, &third, &internal};
    const std::vector<const fpt::SkeletonBranch*> permuted = {
        &internal, &third, &first, &second};
    const fpt::CandidateRootGroupingResult orderedResult =
        fpt::computeCandidateRootGrouping(ordered, {junction});
    const fpt::CandidateRootGroupingResult permutedResult =
        fpt::computeCandidateRootGrouping(permuted, {junction});

    require(
        normalizedCandidateRootPartitions(ordered, orderedResult) ==
            normalizedCandidateRootPartitions(permuted, permutedResult),
        "Candidate root grouping must depend on branch identity and evidence, not vector order.");
    require(
        candidateExcludedBranchIds(ordered, orderedResult) ==
            candidateExcludedBranchIds(permuted, permutedResult),
        "Internal connector exclusion must be order invariant.");
}

void testCandidateRootGroupingPreservesFormalPairsOutsideChangedClusters() {
    fpt::SkeletonBranch changedFirst;
    changedFirst.id = 1;
    changedFirst.points = {{10, 10}, {9, 10}};
    fpt::SkeletonBranch changedSecond;
    changedSecond.id = 2;
    changedSecond.points = {{10, 10}, {11, 10}};
    fpt::SkeletonBranch untouchedFirst;
    untouchedFirst.id = 4;
    untouchedFirst.points = {{20, 20}, {21, 20}};
    fpt::SkeletonBranch untouchedSecond;
    untouchedSecond.id = 5;
    untouchedSecond.points = {{21, 20}, {22, 20}};
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &changedFirst,
        &changedSecond,
        &untouchedFirst,
        &untouchedSecond};

    fpt::JunctionAttributionResult changed;
    changed.junctionPoints = {{10, 10}};
    changed.minGridX = 10;
    changed.maxGridX = 10;
    changed.minGridY = 10;
    changed.maxGridY = 10;
    changed.legacyPairs = {{1, 2}};
    changed.pairs = {{1, 2}};
    changed.differsFromLegacy = true;
    changed.acceptedDiffersFromLegacy = true;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(
            branches,
            {changed});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 2}, {4, 5}},
        "Candidate grouping must preserve every formal legacy pair outside explicitly changed junction clusters.");
}

void testCandidateRootGroupingFallsBackForComplexInternalJunction() {
    std::vector<fpt::SkeletonBranch> storage(8);
    const std::vector<int> ids = {1, 2, 3, 9, 10, 11, 12, 13};
    std::vector<const fpt::SkeletonBranch*> branches;
    for (std::size_t index = 0; index < storage.size(); ++index) {
        storage[index].id = ids[index];
        branches.push_back(&storage[index]);
    }

    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {
        {1, 9}, {9, 10}, {10, 11},
        {11, 12}, {12, 13}, {13, 3}};
    junction.legacyUnmatchedBranchIds = {2};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = true;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(branches, {junction});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{
                {1, 3, 9, 10, 11, 12, 13},
                {2}},
        "A junction with excessive internal connector complexity must preserve the formal legacy partition.");
    require(result.complexityFallbackClusterCount == 1,
            "Complexity fallback must be auditable.");
    require(result.appliedDifferingClusterCount == 0,
            "A complex junction must not enter candidate ownership.");
}

void testCandidateRootGroupingFallsBackForBroadJunctionSpan() {
    fpt::SkeletonBranch first;
    first.id = 1;
    fpt::SkeletonBranch second;
    second.id = 2;
    fpt::SkeletonBranch third;
    third.id = 3;
    const std::vector<const fpt::SkeletonBranch*> branches = {
        &first, &second, &third};

    fpt::JunctionAttributionResult junction;
    junction.minGridX = 0;
    junction.maxGridX = 20;
    junction.minGridY = 0;
    junction.maxGridY = 2;
    junction.legacyPairs = {{1, 3}};
    junction.legacyUnmatchedBranchIds = {2};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = true;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(branches, {junction});

    require(
        normalizedCandidateRootPartitions(branches, result) ==
            std::vector<std::vector<int>>{{1, 3}, {2}},
        "A spatially broad junction cluster must preserve the formal legacy partition.");
    require(result.spanFallbackClusterCount == 1,
            "Broad-span fallback must be auditable.");
    require(result.appliedDifferingClusterCount == 0,
        "A broad junction must not enter candidate ownership.");
}

void testCandidateRootGroupingCountsLegacyOnlyUnmatchedInternalBranches() {
    std::vector<fpt::SkeletonBranch> storage(8);
    const std::vector<int> ids = {1, 2, 3, 9, 10, 11, 12, 13};
    std::vector<const fpt::SkeletonBranch*> branches;
    for (std::size_t index = 0; index < storage.size(); ++index) {
        storage[index].id = ids[index];
        branches.push_back(&storage[index]);
    }

    fpt::JunctionAttributionResult junction;
    junction.legacyPairs = {{1, 3}};
    junction.legacyUnmatchedBranchIds = {
        2, 9, 10, 11, 12, 13};
    junction.pairs = {{1, 2}};
    junction.unmatchedBranchIds = {3};
    junction.differsFromLegacy = true;
    junction.acceptedDiffersFromLegacy = true;

    const fpt::CandidateRootGroupingResult result =
        fpt::computeCandidateRootGrouping(branches, {junction});

    require(result.complexityFallbackClusterCount == 1,
            "Legacy-only unmatched connector branches must contribute to junction complexity.");
    require(result.appliedDifferingClusterCount == 0,
            "A junction with many legacy-only unmatched internal branches must fall back.");
}

} // namespace

int main() {
    try {
        testStraightContinuationIsPaired();
        testXJunctionUsesOpposingPairs();
        testTJunctionLeavesSideBranchUnmatched();
        testNonSymmetricYSelectsBestContinuation();
        testSymmetricYIsReportedAsAmbiguous();
        testGlobalMatchingBeatsInputOrderGreedyChoice();
        testLowSupportShortBranchCannotStealMainContinuation();
        testInputPermutationDoesNotChangeMatching();
        testDirectionUsesMultipleWindowsAndPhysicalSpacing();
        testAdjacentJunctionPixelsAreAnalyzedAsOneCluster();
        testJunctionWorldCenterUsesCellCenterCoordinates();
        testLegacyGreedyComparisonRetainsJunctionEvidence();
        testAmbiguousDifferenceIsCandidateEvidenceNotAcceptedDifference();
        testAdjacentJunctionConnectorProducesAuditableLegacyDifference();
        testSharedLegacyKernelPreservesLaterEqualAngleCandidateAndInputOrder();
        testSharedLegacyKernelPreservesConnectorMediatedRootPartition();
        testLegacyGreedyAndGlobalMatchingCanProduceDifferentRootPartitions();
        testMatcherRejectsUnsafeOptions();
        testCandidateRootGroupingAppliesOnlyAcceptedConnectivityDifference();
        testCandidateRootGroupingFallsBackForAmbiguousDifference();
        testCandidateRootGroupingFallsBackForNonExactMatching();
        testCandidateRootGroupingKeepsLegacyForRepresentationOnlyDifference();
        testCandidateRootGroupingIsInvariantToBranchVectorOrder();
        testCandidateRootGroupingPreservesFormalPairsOutsideChangedClusters();
        testCandidateRootGroupingFallsBackForComplexInternalJunction();
        testCandidateRootGroupingFallsBackForBroadJunctionSpan();
        testCandidateRootGroupingCountsLegacyOnlyUnmatchedInternalBranches();
        std::cout << "test_branch_attribution: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_branch_attribution: FAIL: " << error.what() << "\n";
        return 1;
    }
}
