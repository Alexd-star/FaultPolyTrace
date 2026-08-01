#include "../src/core/SkeletonGraphV2.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::GridMetric metricFor(int width, int height, double dx = 1.0, double dy = 1.0) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    metadata.dx = dx;
    metadata.dy = dy;
    return fpt::GridMetric(metadata);
}

void testSimpleLineBuildsOnePhysicalBranch() {
    fpt::BinaryMask mask(8, 7);
    for (int x = 1; x <= 5; ++x) {
        mask.set(x, 3, true);
    }
    const fpt::SkeletonGraphV2 graph =
        fpt::buildSkeletonGraphV2(mask, metricFor(8, 7, 10.0, 20.0));
    require(graph.stats.endpointPixels == 2, "A simple line must have two endpoint nodes.");
    require(graph.stats.branches == 1, "A simple line must become one branch.");
    require(graph.stats.ports == 2 && graph.stats.halfEdges == 2,
            "Each branch must expose two ports and two half-edges.");
    require(std::abs(graph.branches.front().physicalLength - 40.0) < 1e-9,
            "Branch length must use physical x spacing.");
    require(graph.halfEdges[0].twinId == graph.halfEdges[1].id &&
                graph.halfEdges[1].twinId == graph.halfEdges[0].id,
            "The two branch half-edges must be twins.");
}

void testJunctionPixelsCollapseToOneNode() {
    fpt::BinaryMask mask(12, 10);
    for (int y = 1; y <= 5; ++y) {
        mask.set(5, y, true);
    }
    for (int x = 2; x <= 8; ++x) {
        mask.set(x, 5, true);
    }
    const fpt::SkeletonGraphV2 graph =
        fpt::buildSkeletonGraphV2(mask, metricFor(12, 10));
    require(graph.stats.rawJunctionPixels > 1,
            "The digital T junction should contain multiple raw junction pixels.");
    require(graph.stats.junctionClusters == 1,
            "Adjacent raw junction pixels must collapse to one topology node.");
    require(graph.stats.endpointPixels == 3,
            "The T junction must retain its three external endpoints.");
    require(graph.stats.branches == 3,
            "The clustered T junction must expose exactly three external branches.");
}

void testPureLoopGetsExplicitLoopNodeAndSelfBranch() {
    fpt::BinaryMask mask(7, 7);
    const fpt::SkeletonPoint points[] = {
        {3, 1}, {4, 2}, {5, 3}, {4, 4},
        {3, 5}, {2, 4}, {1, 3}, {2, 2}
    };
    for (const fpt::SkeletonPoint& point : points) {
        mask.set(point.x, point.y, true);
    }
    const fpt::SkeletonGraphV2 graph =
        fpt::buildSkeletonGraphV2(mask, metricFor(7, 7));
    require(graph.stats.endpointPixels == 0 && graph.stats.rawJunctionPixels == 0,
            "A pure loop has no endpoint or raw junction pixels.");
    require(graph.stats.loopNodes == 1 && graph.stats.branches == 1,
            "A pure loop must be represented by one loop node and one branch.");
    require(graph.branches.front().closedLoop,
            "The pure-loop branch must carry explicit closed-loop semantics.");
    require(graph.branches.front().startNodeId == graph.branches.front().endNodeId,
            "The pure-loop branch must be a self-loop in the half-edge graph.");
}

void testDirectlyAdjacentEndpointNodesStillCreateBranch() {
    fpt::BinaryMask mask(5, 5);
    mask.set(2, 2, true);
    mask.set(3, 2, true);
    const fpt::SkeletonGraphV2 graph =
        fpt::buildSkeletonGraphV2(mask, metricFor(5, 5));
    require(graph.stats.endpointPixels == 2,
            "Two adjacent pixels are two endpoint nodes.");
    require(graph.stats.branches == 1,
            "A direct node-to-node skeleton edge must not disappear.");
}

void testDimensionMismatchIsRejected() {
    fpt::BinaryMask mask(5, 5);
    bool rejected = false;
    try {
        (void)fpt::buildSkeletonGraphV2(mask, metricFor(6, 5));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "SkeletonGraphV2 must reject mismatched grid dimensions.");
}

} // namespace

int main() {
    testSimpleLineBuildsOnePhysicalBranch();
    testJunctionPixelsCollapseToOneNode();
    testPureLoopGetsExplicitLoopNodeAndSelfBranch();
    testDirectlyAdjacentEndpointNodesStillCreateBranch();
    testDimensionMismatchIsRejected();
    std::cout << "SkeletonGraphV2 tests passed.\n";
    return 0;
}
