#include "../src/core/WeakContinuityConnector.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
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

void testNonFinitePathSamplesCannotBecomeHighSupport() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 15, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 15, 22, 0.95);
    setLine(skeleton, grid, 24, 22, 36, 22, 0.95);
    setLine(attributeMask, grid, 24, 22, 36, 22, 0.95);
    for (int x = 16; x < 24; ++x) {
        grid.value(x, 22) = std::numeric_limits<double>::quiet_NaN();
    }

    fpt::WeakContinuityOptions options = testOptions();
    options.minSupportRatio = 0.45;
    options.minMeanSupport = 0.25;
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, options);

    require(result.stats.acceptedConnections == 0,
            "NaN path samples must not be interpreted as high support.");
    require(result.stats.invalidPathSamples > 0,
            "NaN path samples must be reported in diagnostics.");
}

void testAllInvalidLocalBackgroundIsRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 15, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 15, 22, 0.95);
    setLine(skeleton, grid, 24, 22, 36, 22, 0.95);
    setLine(attributeMask, grid, 24, 22, 36, 22, 0.95);
    std::fill(grid.values().begin(), grid.values().end(),
              std::numeric_limits<double>::quiet_NaN());
    for (int x = 15; x <= 24; ++x) {
        grid.value(x, 22) = 0.55;
    }

    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, testOptions());

    require(result.stats.acceptedConnections == 0,
            "A candidate without finite local background samples must be rejected.");
    require(result.stats.invalidBackgroundRejected > 0,
            "Invalid local background rejection must be counted.");
}

void testNonFiniteOptionsAreRejected() {
    fpt::WeakContinuityOptions options = testOptions();
    options.minSupportRatio = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try {
        fpt::validateWeakContinuityOptions(options);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Non-finite weak continuity options must be rejected before processing.");
}

void testExistingSkeletonSupportCanBeExcluded() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 18, 22, 30, 22, 0.95);
    setLine(attributeMask, grid, 18, 22, 30, 22, 0.95);
    grid.value(17, 22) = 0.0;

    fpt::WeakContinuityOptions compatibility = testOptions();
    compatibility.minSupportRatio = 0.50;
    compatibility.minMeanSupport = 0.40;
    compatibility.excludeExistingSkeletonFromSupport = false;
    const fpt::WeakContinuityResult legacyResult = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, compatibility);
    require(legacyResult.stats.acceptedConnections == 1,
            "Compatibility statistics should retain endpoint support behavior.");

    fpt::WeakContinuityOptions strict = compatibility;
    strict.excludeExistingSkeletonFromSupport = true;
    const fpt::WeakContinuityResult strictResult = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, strict);
    require(strictResult.stats.acceptedConnections == 0,
            "Strict support statistics must reject an endpoint-only bridge.");
    require(strictResult.stats.excludedSkeletonSupportPixels > 0,
            "Excluded endpoint support pixels must be reported.");
}

void testPhysicalMetricConnectsUsingWorldDistanceAndDirection() {
    fpt::GridMetadata metadata;
    metadata.nx = 64;
    metadata.ny = 48;
    metadata.dx = 10.0;
    metadata.dy = 20.0;
    fpt::Grid2D grid(metadata);
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 25, 22, 38, 22, 0.95);
    setLine(attributeMask, grid, 25, 22, 38, 22, 0.95);
    for (int x = 17; x < 25; ++x) {
        grid.value(x, 22) = 0.58;
    }
    fpt::WeakContinuityOptions options = testOptions();
    options.usePhysicalMetric = true;
    options.maxDistance = 10;
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, options);
    require(result.stats.acceptedConnections == 1,
            "A physical straight weak gap should remain connectable on an anisotropic grid.");
    require(!result.candidates.empty() && result.candidates.front().endpointDistanceWorld > 0.0,
            "Physical weak-path candidates must report world endpoint distance.");
}

void testGlobalMatchingCanChooseNoConnect() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 25, 22, 38, 22, 0.95);
    setLine(attributeMask, grid, 25, 22, 38, 22, 0.95);
    for (int x = 17; x < 25; ++x) {
        grid.value(x, 22) = 0.58;
    }
    fpt::WeakContinuityOptions options = testOptions();
    options.useGlobalMatching = true;
    options.noConnectCost = 0.001;
    options.minSupportRatio = 0.10;
    options.minMeanSupport = 0.05;
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, options);
    require(result.stats.globalMatchingExactComponents == 1,
            "The small candidate component must use exact global matching.");
    require(result.stats.acceptedConnections == 0,
            "Global matching must be able to select the no-connect alternative.");
    require(result.stats.globalMatchingNoConnectEndpoints >= 2,
            "No-connect endpoint choices must be reported.");
}

void testGlobalMatchingRejectsCrossingCandidatePaths() {
    fpt::Grid2D grid = makeGrid(56, 56);
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 28, 18, 28, 0.95);
    setLine(attributeMask, grid, 4, 28, 18, 28, 0.95);
    setLine(skeleton, grid, 38, 28, 51, 28, 0.95);
    setLine(attributeMask, grid, 38, 28, 51, 28, 0.95);
    setLine(skeleton, grid, 28, 4, 28, 18, 0.95);
    setLine(attributeMask, grid, 28, 4, 28, 18, 0.95);
    setLine(skeleton, grid, 28, 38, 28, 51, 0.95);
    setLine(attributeMask, grid, 28, 38, 28, 51, 0.95);
    for (int index = 19; index < 38; ++index) {
        grid.value(index, 28) = 0.62;
        grid.value(28, index) = 0.62;
    }

    fpt::WeakContinuityOptions options = testOptions();
    options.maxDistance = 24;
    options.corridorRadius = 5;
    options.maxCandidatesPerEndpoint = 2;
    options.maxEndpointAngleDegrees = 30.0;
    options.useGlobalMatching = true;
    options.noConnectCost = 10.0;
    const fpt::WeakContinuityResult result = fpt::connectWeakContinuityPaths(
        grid, skeleton, attributeMask, fpt::buildSkeletonGraph(skeleton), 0.80, options);

    require(result.stats.spatialConflictPairs >= 1,
            "Crossing weak-path candidates must form a spatial conflict pair.");
    require(result.stats.acceptedConnections == 1,
            "At most one of two crossing candidates may be selected.");
    require(result.stats.spatialConflictRejected >= 1,
            "The rejected crossing candidate must be visible in diagnostics.");
}

void testBoundaryAdjustedDistanceFindsWideEndCaps() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 31, 22, 43, 22, 0.95);
    for (int y = 20; y <= 24; ++y) {
        for (int x = 4; x <= 18; ++x) {
            attributeMask.set(x, y, true);
            grid.value(x, y) = 0.95;
        }
        for (int x = 29; x <= 43; ++x) {
            attributeMask.set(x, y, true);
            grid.value(x, y) = 0.95;
        }
    }
    for (int x = 19; x < 29; ++x) {
        grid.value(x, 22) = 0.58;
    }

    fpt::WeakContinuityOptions strict = testOptions();
    strict.maxDistance = 12;
    strict.minSupportRatio = 0.45;
    strict.minMeanSupport = 0.30;
    const fpt::WeakContinuityResult centerDistance =
        fpt::connectWeakContinuityPaths(
            grid, skeleton, attributeMask,
            fpt::buildSkeletonGraph(skeleton), 0.80, strict);
    require(centerDistance.stats.preselectedPairs == 0,
            "Centerline endpoints beyond the limit must not be preselected.");

    strict.useBoundaryAdjustedCandidateDistance = true;
    const fpt::WeakContinuityResult adjusted =
        fpt::connectWeakContinuityPaths(
            grid, skeleton, attributeMask,
            fpt::buildSkeletonGraph(skeleton), 0.80, strict);
    require(adjusted.stats.preselectedPairs == 1,
            "Wide end caps must use their boundary-adjusted gap distance.");
    require(!adjusted.candidates.empty() &&
            adjusted.candidates.front().boundaryAdjustedDistance <= 12.0,
            "Adjusted candidate distance must be reported.");
}

void testSparseNeighborhoodSupportRescuesAlignedGap() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 27, 22, 40, 22, 0.95);
    setLine(attributeMask, grid, 27, 22, 40, 22, 0.95);
    for (int x = 17; x < 27; ++x) {
        grid.value(x, 23) = 0.62;
        if ((x - 17) % 4 == 0) {
            grid.value(x, 22) = 0.62;
        }
    }

    fpt::WeakContinuityOptions options = testOptions();
    options.minSupportRatio = 0.65;
    options.minMeanSupport = 0.52;
    options.maxUnsupportedRun = 2;
    options.excludeExistingSkeletonFromSupport = true;
    options.attributeWeight = 0.0;
    const fpt::WeakContinuityResult exactOnly =
        fpt::connectWeakContinuityPaths(
            grid, skeleton, attributeMask,
            fpt::buildSkeletonGraph(skeleton), 0.80, options);
    require(exactOnly.stats.acceptedConnections == 0,
            "An unsupported exact path must remain rejected by default.");

    options.supportNeighborhoodRadius = 1;
    const fpt::WeakContinuityResult neighborhood =
        fpt::connectWeakContinuityPaths(
            grid, skeleton, attributeMask,
            fpt::buildSkeletonGraph(skeleton), 0.80, options);
    require(neighborhood.stats.acceptedConnections == 1,
            "A strictly aligned point-sampled weak path should be recoverable.");
    require(!neighborhood.candidates.empty() &&
            neighborhood.candidates.front().usedNeighborhoodSupport,
            "Neighborhood-supported acceptance must be explicit in diagnostics.");
    require(neighborhood.candidates.front().exactSupportRatio <
            neighborhood.candidates.front().neighborhoodSupportRatio,
            "Exact and neighborhood support evidence must remain distinguishable.");
}

void testNearbyParallelSkeletonCannotLendNeighborhoodSupport() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask attributeMask(grid.nx(), grid.ny());
    setLine(skeleton, grid, 4, 22, 16, 22, 0.95);
    setLine(attributeMask, grid, 4, 22, 16, 22, 0.95);
    setLine(skeleton, grid, 27, 22, 40, 22, 0.95);
    setLine(attributeMask, grid, 27, 22, 40, 22, 0.95);
    setLine(skeleton, grid, 18, 23, 25, 23, 0.62);
    setLine(attributeMask, grid, 18, 23, 25, 23, 0.62);

    fpt::WeakContinuityOptions options = testOptions();
    options.minSupportRatio = 0.65;
    options.minMeanSupport = 0.52;
    options.maxUnsupportedRun = 2;
    options.excludeExistingSkeletonFromSupport = true;
    options.supportNeighborhoodRadius = 1;
    options.thirdPartySkeletonClearanceRadius = 0;
    options.attributeWeight = 0.0;
    const fpt::WeakContinuityResult result =
        fpt::connectWeakContinuityPaths(
            grid, skeleton, attributeMask,
            fpt::buildSkeletonGraph(skeleton), 0.80, options);

    bool inspectedMainGap = false;
    for (const fpt::WeakContinuityCandidate& candidate : result.candidates) {
        const bool mainGap =
            (candidate.endpointA.x == 16 && candidate.endpointA.y == 22 &&
             candidate.endpointB.x == 27 && candidate.endpointB.y == 22) ||
            (candidate.endpointB.x == 16 && candidate.endpointB.y == 22 &&
             candidate.endpointA.x == 27 && candidate.endpointA.y == 22);
        if (!mainGap) {
            continue;
        }
        inspectedMainGap = true;
        require(!candidate.accepted && !candidate.usedNeighborhoodSupport,
            "A nearby parallel skeleton must not rescue an unsupported gap.");
        require(candidate.neighborhoodSupportRatio < options.minSupportRatio,
            "Third-party skeleton samples must be excluded from neighborhood evidence.");
    }
    require(inspectedMainGap,
        "The main parallel-support test gap must be evaluated.");
}

} // namespace

int main() {
    testStraightPointSupportedGapConnects();
    testCurvedWeakPathIsFollowed();
    testBlankGapIsRejected();
    testParallelTracesDoNotCrossConnect();
    testThirdPartySkeletonCrossingIsRejected();
    testDefaultOptionsAreConservativeAndValid();
    testNonFinitePathSamplesCannotBecomeHighSupport();
    testAllInvalidLocalBackgroundIsRejected();
    testNonFiniteOptionsAreRejected();
    testExistingSkeletonSupportCanBeExcluded();
    testPhysicalMetricConnectsUsingWorldDistanceAndDirection();
    testGlobalMatchingCanChooseNoConnect();
    testGlobalMatchingRejectsCrossingCandidatePaths();
    testBoundaryAdjustedDistanceFindsWideEndCaps();
    testSparseNeighborhoodSupportRescuesAlignedGap();
    testNearbyParallelSkeletonCannotLendNeighborhoodSupport();
    std::cout << "Weak continuity connector tests passed.\n";
    return 0;
}
