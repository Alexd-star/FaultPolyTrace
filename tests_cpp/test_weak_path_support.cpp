#include "../src/core/WeakPathSupport.h"

#include <cmath>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(int width, int height, double value = 0.0) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    fpt::Grid2D grid(metadata);
    for (double& cell : grid.values()) {
        cell = value;
    }
    return grid;
}

fpt::Grid2D makeGrid(
    int width,
    int height,
    double dx,
    double dy,
    double value = 0.0) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    metadata.dx = dx;
    metadata.dy = dy;
    fpt::Grid2D grid(metadata);
    for (double& cell : grid.values()) {
        cell = value;
    }
    return grid;
}

void setHorizontalCandidate(
    fpt::BinaryMask& candidate,
    fpt::Grid2D& grid,
    int x0,
    int x1,
    int y,
    double value) {
    for (int x = x0; x <= x1; ++x) {
        candidate.set(x, y, true);
        grid.value(x, y) = value;
    }
}

void setCandidateCell(
    fpt::BinaryMask& candidate,
    fpt::Grid2D& grid,
    int x,
    int y,
    double value) {
    candidate.set(x, y, true);
    grid.value(x, y) = value;
}

void requireEveryAcceptedCellReachesASeed(
    const fpt::BinaryMask& accepted,
    const fpt::BinaryMask& seeds) {
    const int width = accepted.width();
    const int height = accepted.height();
    std::vector<unsigned char> reachable(
        static_cast<std::size_t>(width * height),
        0);
    std::queue<std::pair<int, int>> queue;
    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}};

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!accepted.get(x, y)) {
                continue;
            }
            bool touchesSeed = seeds.get(x, y);
            for (const auto& offset : offsets) {
                touchesSeed = touchesSeed ||
                    seeds.get(x + offset[0], y + offset[1]);
            }
            if (!touchesSeed) {
                continue;
            }
            const std::size_t position =
                static_cast<std::size_t>(y * width + x);
            reachable[position] = 1;
            queue.push({x, y});
        }
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();
        for (const auto& offset : offsets) {
            const int nextX = current.first + offset[0];
            const int nextY = current.second + offset[1];
            if (!accepted.inBounds(nextX, nextY) ||
                !accepted.get(nextX, nextY)) {
                continue;
            }
            const std::size_t position =
                static_cast<std::size_t>(nextY * width + nextX);
            if (reachable[position]) {
                continue;
            }
            reachable[position] = 1;
            queue.push({nextX, nextY});
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!accepted.get(x, y)) {
                continue;
            }
            require(
                reachable[static_cast<std::size_t>(y * width + x)] != 0,
                "Every accepted weak-path cell must remain connected to a strong seed.");
        }
    }
}

void testLongUnsupportedCorridorIsRejected() {
    fpt::Grid2D grid = makeGrid(32, 7);
    fpt::BinaryMask candidate(32, 7);
    fpt::BinaryMask seeds(32, 7);
    setHorizontalCandidate(candidate, grid, 1, 5, 3, 0.85);
    setHorizontalCandidate(candidate, grid, 6, 19, 3, 0.25);
    setHorizontalCandidate(candidate, grid, 20, 29, 3, 0.80);
    seeds.set(2, 3, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 3;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(result.acceptedMask.get(8, 3), "Short low-support prefix should remain reachable.");
    require(!result.acceptedMask.get(9, 3), "Long unsupported corridor should be cut.");
    require(!result.acceptedMask.get(20, 3), "Remote lobe must not be accepted across a long gap.");
    require(result.stats.rejectedCellCount > 0, "Rejected cells should be reported.");
}

void testShortUnsupportedGapIsAccepted() {
    fpt::Grid2D grid = makeGrid(18, 7);
    fpt::BinaryMask candidate(18, 7);
    fpt::BinaryMask seeds(18, 7);
    setHorizontalCandidate(candidate, grid, 1, 5, 3, 0.85);
    setHorizontalCandidate(candidate, grid, 6, 7, 3, 0.25);
    setHorizontalCandidate(candidate, grid, 8, 14, 3, 0.82);
    seeds.set(2, 3, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 2;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    for (int x = 1; x <= 14; ++x) {
        require(result.acceptedMask.get(x, 3), "A supported path with a short gap should be accepted.");
    }
    require(result.stats.rejectedCellCount == 0, "No cells should be rejected in the legal short-gap case.");
    require(std::abs(result.stats.directionX) > 0.95, "Horizontal direction should be estimated.");
    require(result.stats.majorAxisLength >= 13.0, "Major-axis length should describe the candidate path.");
    require(result.stats.estimatedWidth <= 1.5, "One-cell path should remain narrow.");
}

void testMissingSeedFallsBackToEmptyExtension() {
    fpt::Grid2D grid = makeGrid(12, 7);
    fpt::BinaryMask candidate(12, 7);
    fpt::BinaryMask seeds(12, 7);
    setHorizontalCandidate(candidate, grid, 2, 9, 3, 0.80);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 3;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(result.acceptedMask.countActive() == 0, "Seedless candidates must not be promoted.");
    require(result.stats.seedCellCount == 0, "Seedless evidence should be explicit.");
    require(result.stats.insufficientSeedEvidence, "Fallback reason should be reported.");
}

void testSeedSearchRadiusRejectsUnsupportedValues() {
    fpt::Grid2D grid = makeGrid(8, 5);
    fpt::BinaryMask candidate(8, 5);
    fpt::BinaryMask seeds(8, 5);
    setCandidateCell(candidate, grid, 2, 2, 0.80);
    seeds.set(1, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.seedSearchRadius = 2;
    bool rejected = false;
    try {
        (void)fpt::selectWeakPathSupport(grid, candidate, seeds, options);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(
        rejected,
        "Seed search radius values above one must be rejected instead of silently acting like one.");
}

void testSeedSearchRadiusCannotJumpAcrossNonCandidateBackground() {
    fpt::Grid2D grid = makeGrid(12, 5);
    fpt::BinaryMask candidate(12, 5);
    fpt::BinaryMask seeds(12, 5);
    setHorizontalCandidate(candidate, grid, 3, 8, 2, 0.80);
    seeds.set(1, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 0;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() == 0,
        "Seed search must not jump across cells outside both the seed and candidate masks.");
    require(
        result.stats.seedContactCellCount == 0,
        "A geometrically nearby but disconnected candidate must not be reported as seed-connected.");
}

void testOpposingRootsDoNotReconstructLongUnsupportedBridge() {
    fpt::Grid2D grid = makeGrid(12, 5);
    fpt::BinaryMask candidate(12, 5);
    fpt::BinaryMask seeds(12, 5);
    setHorizontalCandidate(candidate, grid, 2, 7, 2, 0.25);
    seeds.set(1, 2, true);
    seeds.set(8, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 3;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() < 6,
        "Two independent roots must not accept opposite halves of one overlong unsupported bridge.");
    require(
        !result.acceptedMask.get(4, 2) || !result.acceptedMask.get(5, 2),
        "The root-conflict boundary must remain disconnected.");
    require(
        result.stats.rootConflictRejectedCellCount > 0,
        "Root-conflict rejection must be visible in diagnostics.");
}

void testOpposingRootsAcceptLegalShortUnsupportedBridge() {
    fpt::Grid2D grid = makeGrid(7, 5);
    fpt::BinaryMask candidate(7, 5);
    fpt::BinaryMask seeds(7, 5);
    setHorizontalCandidate(candidate, grid, 2, 3, 2, 0.25);
    seeds.set(1, 2, true);
    seeds.set(4, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 2;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.get(2, 2) &&
            result.acceptedMask.get(3, 2),
        "A two-cell unsupported bridge must remain legal when the configured limit is two.");
    require(
        result.stats.rootConflictRejectedCellCount == 0,
        "A legal short bridge must not be reported as a root conflict.");
}

void testRootConflictCutLeavesNoSeedlessAcceptedIsland() {
    fpt::Grid2D grid = makeGrid(7, 7);
    fpt::BinaryMask candidate(7, 7);
    fpt::BinaryMask seeds(7, 7);
    for (int index = 1; index <= 4; ++index) {
        setCandidateCell(candidate, grid, index, index, 0.25);
    }
    setCandidateCell(candidate, grid, 4, 2, 0.80);
    setCandidateCell(candidate, grid, 5, 2, 0.80);
    seeds.set(0, 0, true);
    seeds.set(5, 5, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 2;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    requireEveryAcceptedCellReachesASeed(result.acceptedMask, seeds);
}

void testEvidenceStatisticsCountPostCutFragments() {
    fpt::Grid2D grid = makeGrid(12, 5);
    fpt::BinaryMask candidate(12, 5);
    fpt::BinaryMask seeds(12, 5);
    setHorizontalCandidate(candidate, grid, 2, 7, 2, 0.25);
    seeds.set(1, 2, true);
    seeds.set(8, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 3;
    options.seedSearchRadius = 1;
    options.minEvidenceCells = 1;
    options.minMeanAttribute = 0.0;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.stats.candidateComponentCount == 1,
        "The input remains one candidate component before conflict cutting.");
    require(
        result.stats.evidenceEvaluatedFragmentCount == 2,
        "Evidence statistics must count the two post-cut connected fragments.");
}

void testRootConflictCutRevalidatesUnsupportedRunConstraint() {
    fpt::Grid2D grid = makeGrid(4, 4);
    fpt::BinaryMask candidate(4, 4);
    fpt::BinaryMask seeds(4, 4);
    setCandidateCell(candidate, grid, 2, 1, 0.25);
    setCandidateCell(candidate, grid, 0, 2, 0.25);
    setCandidateCell(candidate, grid, 1, 2, 0.25);
    setCandidateCell(candidate, grid, 1, 3, 0.25);
    setCandidateCell(candidate, grid, 2, 3, 0.80);
    seeds.set(3, 0, true);
    seeds.set(1, 1, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        !result.acceptedMask.get(1, 3),
        "A post-cut path requiring two consecutive unsupported cells must be removed.");
    require(
        !result.acceptedMask.get(2, 3),
        "Cells reachable only through a post-cut invalid unsupported run must also be removed.");
}

void testPartiallyRejectedCandidateComponentIsReported() {
    fpt::Grid2D grid = makeGrid(16, 5);
    fpt::BinaryMask candidate(16, 5);
    fpt::BinaryMask seeds(16, 5);
    setHorizontalCandidate(candidate, grid, 2, 5, 2, 0.85);
    setHorizontalCandidate(candidate, grid, 6, 13, 2, 0.25);
    seeds.set(1, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 2;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.stats.candidateComponentCount == 1,
        "Candidate component count must describe the input candidate mask.");
    require(
        result.stats.acceptedComponentCount == 1,
        "A partially retained input component still has accepted evidence.");
    require(
        result.stats.partiallyRejectedComponentCount == 1,
        "Partial path truncation must be reported explicitly.");
    require(
        result.stats.rejectedComponentCount == 0,
        "A partially retained component must not be reported as fully rejected.");
}

void testDirectionUsesPhysicalGridSpacing() {
    fpt::Grid2D grid = makeGrid(16, 16, 10.0, 1.0);
    fpt::BinaryMask candidate(16, 16);
    fpt::BinaryMask seeds(16, 16);
    for (int index = 2; index <= 12; ++index) {
        setCandidateCell(candidate, grid, index, index, 0.80);
    }
    seeds.set(1, 1, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        std::abs(result.stats.directionX) > 0.99,
        "Direction evidence must use physical grid spacing instead of raw pixel indices.");
    require(
        std::abs(result.stats.directionY) < 0.15,
        "An anisotropic grid must report the physically dominant direction.");
}

void testWidthEvidenceUsesCellUnitsOnAnisotropicGrid() {
    fpt::Grid2D grid = makeGrid(12, 18, 100.0, 1.0);
    fpt::BinaryMask candidate(12, 18);
    fpt::BinaryMask seeds(12, 18);
    for (int y = 2; y <= 14; ++y) {
        setCandidateCell(candidate, grid, 5, y, 0.80);
    }
    seeds.set(5, 1, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    options.seedSearchRadius = 1;
    options.minEvidenceCells = 5;
    options.maxEstimatedWidth = 2.0;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() == 13,
        "A one-cell-wide path must not fail width evidence only because dx and dy differ.");
    require(
        result.stats.maxObservedComponentWidth <= 2.0,
        "Estimated width must use cell units because the configured threshold is expressed in cells.");
}

void testNonFiniteCandidateCellsAreNeverAccepted() {
    fpt::Grid2D grid = makeGrid(10, 5);
    fpt::BinaryMask candidate(10, 5);
    fpt::BinaryMask seeds(10, 5);
    setCandidateCell(
        candidate,
        grid,
        2,
        2,
        std::numeric_limits<double>::quiet_NaN());
    setCandidateCell(candidate, grid, 3, 2, 0.80);
    seeds.set(1, 2, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 2;
    options.seedSearchRadius = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        !result.acceptedMask.get(2, 2),
        "A non-finite candidate cell must never enter the accepted path.");
    require(
        !result.acceptedMask.get(3, 2),
        "A finite remote cell must not be reached through a non-finite candidate.");
}

void testWideBlobFailsPathShapeEvidence() {
    fpt::Grid2D grid = makeGrid(16, 16);
    fpt::BinaryMask candidate(16, 16);
    fpt::BinaryMask seeds(16, 16);
    for (int y = 4; y <= 10; ++y) {
        for (int x = 4; x <= 10; ++x) {
            setCandidateCell(candidate, grid, x, y, 0.80);
        }
    }
    seeds.set(7, 7, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    options.minEvidenceCells = 5;
    options.minAspectRatio = 3.0;
    options.maxEstimatedWidth = 3.0;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() == 0,
        "A compact wide blob must not be promoted as a fault-like weak path.");
    require(
        result.stats.shapeRejectedFragmentCount == 1,
        "Wide-blob rejection should be reported as shape evidence.");
}

void testSharpTurnFailsDirectionEvidence() {
    fpt::Grid2D grid = makeGrid(20, 20);
    fpt::BinaryMask candidate(20, 20);
    fpt::BinaryMask seeds(20, 20);
    setHorizontalCandidate(candidate, grid, 2, 8, 5, 0.80);
    setCandidateCell(candidate, grid, 9, 6, 0.80);
    for (int y = 7; y <= 14; ++y) {
        setCandidateCell(candidate, grid, 9, y, 0.80);
    }
    seeds.set(2, 5, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    options.minEvidenceCells = 5;
    options.maxTurnAngleDegrees = 45.0;
    options.directionWindow = 2;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() == 0,
        "A near-right-angle path must fail a strict direction-continuity gate.");
    require(
        result.stats.directionRejectedFragmentCount == 1,
        "Sharp-turn rejection should be reported as direction evidence.");
}

void testLowMeanPathFailsAttributeEvidence() {
    fpt::Grid2D grid = makeGrid(18, 7);
    fpt::BinaryMask candidate(18, 7);
    fpt::BinaryMask seeds(18, 7);
    setHorizontalCandidate(candidate, grid, 2, 14, 3, 0.55);
    seeds.set(2, 3, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 20;
    options.minEvidenceCells = 5;
    options.minMeanAttribute = 0.58;
    options.minLowerQuantileAttribute = 0.50;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() == 0,
        "A long path with insufficient mean support must be rejected.");
    require(
        result.stats.attributeRejectedFragmentCount == 1,
        "Low-mean rejection should be reported as attribute evidence.");
}

void testStraightPathPassesCombinedEvidence() {
    fpt::Grid2D grid = makeGrid(20, 9);
    fpt::BinaryMask candidate(20, 9);
    fpt::BinaryMask seeds(20, 9);
    setHorizontalCandidate(candidate, grid, 2, 16, 4, 0.80);
    seeds.set(2, 4, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    options.minEvidenceCells = 5;
    options.minMeanAttribute = 0.70;
    options.minLowerQuantileAttribute = 0.70;
    options.minAspectRatio = 3.0;
    options.maxEstimatedWidth = 3.0;
    options.maxTurnAngleDegrees = 45.0;
    options.directionWindow = 2;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.acceptedMask.countActive() == 15,
        "A straight, narrow, well-supported path must pass all evidence gates.");
    require(
        result.stats.acceptedComponentCount == 1,
        "The passing path component should be reported.");
}

void testDisconnectedPathsAreCountedIndependently() {
    fpt::Grid2D grid = makeGrid(20, 12);
    fpt::BinaryMask candidate(20, 12);
    fpt::BinaryMask seeds(20, 12);
    setHorizontalCandidate(candidate, grid, 2, 8, 3, 0.80);
    setHorizontalCandidate(candidate, grid, 10, 16, 8, 0.80);
    seeds.set(2, 3, true);
    seeds.set(10, 8, true);

    fpt::WeakPathSupportOptions options;
    options.supportThreshold = 0.60;
    options.maxUnsupportedRun = 1;
    const fpt::WeakPathSupportResult result =
        fpt::selectWeakPathSupport(grid, candidate, seeds, options);

    require(
        result.stats.candidateComponentCount == 2,
        "Disconnected weak paths must be evaluated as separate components.");
    require(
        result.stats.acceptedComponentCount == 2,
        "Both independently supported paths should be accepted.");
}

} // namespace

int main() {
    try {
        testLongUnsupportedCorridorIsRejected();
        testShortUnsupportedGapIsAccepted();
        testMissingSeedFallsBackToEmptyExtension();
        testSeedSearchRadiusRejectsUnsupportedValues();
        testSeedSearchRadiusCannotJumpAcrossNonCandidateBackground();
        testOpposingRootsDoNotReconstructLongUnsupportedBridge();
        testOpposingRootsAcceptLegalShortUnsupportedBridge();
        testRootConflictCutLeavesNoSeedlessAcceptedIsland();
        testEvidenceStatisticsCountPostCutFragments();
        testRootConflictCutRevalidatesUnsupportedRunConstraint();
        testPartiallyRejectedCandidateComponentIsReported();
        testDirectionUsesPhysicalGridSpacing();
        testWidthEvidenceUsesCellUnitsOnAnisotropicGrid();
        testNonFiniteCandidateCellsAreNeverAccepted();
        testWideBlobFailsPathShapeEvidence();
        testSharpTurnFailsDirectionEvidence();
        testLowMeanPathFailsAttributeEvidence();
        testStraightPathPassesCombinedEvidence();
        testDisconnectedPathsAreCountedIndependently();
        std::cout << "test_weak_path_support: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_weak_path_support: FAIL: " << error.what() << "\n";
        return 1;
    }
}
