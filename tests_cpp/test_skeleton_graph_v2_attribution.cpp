#include "../src/core/SkeletonGraphV2Attribution.h"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 25;
    metadata.ny = 25;
    metadata.dx = 10.0;
    metadata.dy = 10.0;
    return fpt::Grid2D(metadata);
}

void testJunctionLevelComparisonIsReported() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    for (int x = 3; x <= 21; ++x) {
        skeleton.set(x, 12, true);
        grid.value(x, 12) = 1.0;
    }
    for (int y = 3; y <= 21; ++y) {
        skeleton.set(12, y, true);
        grid.value(12, y) = 1.0;
    }
    const fpt::SkeletonGraph legacyGraph = fpt::buildSkeletonGraph(skeleton);
    std::vector<const fpt::SkeletonBranch*> legacyRefs;
    for (const fpt::SkeletonBranch& branch : legacyGraph.branches) {
        legacyRefs.push_back(&branch);
    }
    const fpt::SkeletonGraphV2 graphV2 = fpt::buildSkeletonGraphV2(
        skeleton, fpt::GridMetric(grid));
    fpt::BranchAttributionOptions options;
    options.supportThreshold = 0.5;
    const fpt::SkeletonGraphV2AttributionComparison comparison =
        fpt::compareSkeletonGraphV2Attribution(
            grid, legacyRefs, graphV2, options);

    require(comparison.evaluated,
            "V2 branch attribution comparison must be marked as evaluated.");
    require(comparison.legacyJunctionClusters >= 1 &&
                comparison.v2JunctionClusters >= 1,
            "Both graph representations must retain the central junction.");
    require(comparison.matchedJunctionClusters >= 1,
            "Nearby legacy and V2 junctions must be compared individually.");
    require(comparison.equivalentMatchedJunctionClusters +
                comparison.differingMatchedJunctionClusters ==
                comparison.matchedJunctionClusters,
            "Every matched junction must have an explicit A/B classification.");
}

} // namespace

int main() {
    testJunctionLevelComparisonIsReported();
    std::cout << "SkeletonGraphV2 attribution tests passed.\n";
    return 0;
}
