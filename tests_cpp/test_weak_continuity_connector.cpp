#include "../src/core/WeakContinuityConnector.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(int width = 64, int height = 48) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    return fpt::Grid2D(metadata);
}

void setLine(fpt::BinaryMask& mask, fpt::Grid2D& grid,
             int x0, int y0, int x1, int y1, double value) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    while (true) {
        mask.set(x0, y0, true);
        grid.value(x0, y0) = value;
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twice = error * 2;
        if (twice >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

fpt::WeakContinuityOptions testOptions() {
    fpt::WeakContinuityOptions options;
    options.maxDistance = 28;
    options.tangentWindow = 5;
    options.corridorRadius = 7;
    options.maxEndpointAngleDegrees = 38.0;
    options.minAttributeRatio = 0.35;
    options.minSupportRatio = 0.35;
    options.minMeanSupport = 0.20;
    options.maxUnsupportedRun = 3;
    options.minCompetitionMargin = 0.04;
    return options;
}

void testStraightPointSupportedGapConnects() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 25, 22, 38, 22, 0.95);
    setLine(attributeMask, grid, 25, 22, 38, 22, 0.95);
    for (int x = 17; x < 25; ++x) {
        grid.value(x, 22) = (x == 20) ? 0.0 : 0.52;
    }
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, testOptions());
    require(result.stats.acceptedConnections == 1, "Supported straight gap should connect once.");
    require(result.mask.get(20, 22), "Accepted path must bridge the point interruption.");
}

void testCurvedWeakPathIsFollowed() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 30, 15, 30, 0.95);
    setLine(attributeMask, grid, 4, 30, 15, 30, 0.95);
    setLine(skeleton, grid, 29, 27, 41, 24, 0.95);
    setLine(attributeMask, grid, 29, 27, 41, 24, 0.95);
    const int weak[][2] = {{16,30},{17,30},{18,30},{19,29},{20,29},{21,29},
                           {22,28},{23,28},{24,28},{25,27},{26,27},{27,27},{28,27}};
    for (const auto& point : weak) {
        grid.value(point[0], point[1]) = 0.58;
    }
    fpt::WeakContinuityOptions options = testOptions();
    options.maxEndpointAngleDegrees = 48.0;
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, options);
    require(result.stats.acceptedConnections == 1, "Gently curved supported gap should connect.");
    bool followedCurve = false;
    for (const fpt::WeakContinuityCandidate& candidate : result.candidates) {
        if (!candidate.accepted) {
            continue;
        }
        for (const fpt::SkeletonPoint& point : candidate.path) {
            followedCurve = followedCurve || point.y < 30;
        }
    }
    require(followedCurve, "Weak path search should follow the curved attribute evidence.");
}

void testBlankGapIsRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 12, 15, 12, 0.95);
    setLine(attributeMask, grid, 4, 12, 15, 12, 0.95);
    setLine(skeleton, grid, 30, 12, 42, 12, 0.95);
    setLine(attributeMask, grid, 30, 12, 42, 12, 0.95);
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, testOptions());
    require(result.stats.acceptedConnections == 0, "Long blank gap must be rejected.");
}

void testParallelTracesDoNotCrossConnect() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 6, 16, 35, 16, 0.95);
    setLine(attributeMask, grid, 6, 16, 35, 16, 0.95);
    setLine(skeleton, grid, 6, 22, 35, 22, 0.95);
    setLine(attributeMask, grid, 6, 22, 35, 22, 0.95);
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, testOptions());
    require(result.stats.acceptedConnections == 0, "Parallel traces must not be connected laterally.");
}

void testThirdPartySkeletonCrossingIsRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 15, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 15, 22, 0.95);
    setLine(skeleton, grid, 30, 22, 42, 22, 0.95);
    setLine(attributeMask, grid, 30, 22, 42, 22, 0.95);
    setLine(skeleton, grid, 22, 8, 22, 36, 0.95);
    setLine(attributeMask, grid, 22, 8, 22, 36, 0.95);
    for (int x = 16; x < 30; ++x) {
        grid.value(x, 22) = 0.60;
    }
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, testOptions());
    require(result.stats.acceptedConnections == 0,
            "A weak path crossing an unrelated skeleton must be rejected.");
}

void testDefaultOptionsAreConservativeAndValid() {
    fpt::WeakContinuityOptions options;
    fpt::validateWeakContinuityOptions(options);
    require(options.maxDistance > 12, "Candidate mode should examine gaps beyond the legacy 12-cell limit.");
    require(options.minSupportRatio > 0.0, "Candidate mode must require attribute support.");
    require(options.maxUnsupportedRun < options.maxDistance, "Candidate mode must limit blank runs.");
}

} // namespace

int main() {
    testStraightPointSupportedGapConnects();
    testCurvedWeakPathIsFollowed();
    testBlankGapIsRejected();
    testParallelTracesDoNotCrossConnect();
    testThirdPartySkeletonCrossingIsRejected();
    testDefaultOptionsAreConservativeAndValid();
    std::cout << "Weak continuity connector tests passed.\n";
    return 0;
}
