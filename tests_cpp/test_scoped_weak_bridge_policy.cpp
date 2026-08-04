#include "../src/core/ScopedWeakBridgePolicy.h"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(double dx = 1.0, double dy = 1.0) {
    fpt::GridMetadata metadata;
    metadata.nx = 48;
    metadata.ny = 32;
    metadata.dx = dx;
    metadata.dy = dy;
    return fpt::Grid2D(metadata);
}

fpt::WeakContinuityResult makeExpandedPass() {
    fpt::WeakContinuityResult pass;
    pass.mask = fpt::BinaryMask(48, 32);
    fpt::WeakContinuityCandidate candidate;
    candidate.accepted = true;
    candidate.selectedByGlobalMatching = true;
    candidate.decision = "accepted";
    candidate.endpointA = {8, 16};
    candidate.endpointB = {21, 16};
    candidate.endpointDistance = 13.0;
    candidate.addedPathPixels = 12;
    for (int x = 8; x <= 21; ++x) {
        candidate.path.push_back({x, 16});
    }
    pass.candidates.push_back(candidate);
    pass.stats.acceptedConnections = 1;
    pass.stats.addedPathPixels = candidate.addedPathPixels;
    return pass;
}

fpt::ScopedWeakBridgeOptions testOptions() {
    fpt::ScopedWeakBridgeOptions options;
    options.enabled = true;
    options.baselineMaxDistance = 12.0;
    options.expandedMaxDistance = 14.0;
    options.splitInfluenceRadiusWorld = 3.0;
    return options;
}

void testExpandedBridgeNearCommittedSplitIsAccepted() {
    const fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask base(grid.nx(), grid.ny());
    fpt::BinaryMask split(grid.nx(), grid.ny());
    split.set(8, 18, true);
    auto pass = makeExpandedPass();
    const auto stats = fpt::applyScopedWeakBridgePolicy(
        grid, base, split, pass, testOptions());
    require(pass.candidates.front().accepted && stats.acceptedNearSplit == 1,
        "An expanded bridge near a committed split should remain eligible.");
    require(pass.mask.get(15, 16),
        "The accepted path must be represented in the rebuilt pass mask.");
}

void testIdenticalBridgeAwayFromSplitIsRejected() {
    const fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask base(grid.nx(), grid.ny());
    fpt::BinaryMask split(grid.nx(), grid.ny());
    split.set(40, 28, true);
    auto pass = makeExpandedPass();
    const auto stats = fpt::applyScopedWeakBridgePolicy(
        grid, base, split, pass, testOptions());
    require(!pass.candidates.front().accepted &&
            pass.candidates.front().decision ==
                "outside_local_split_influence",
        "The same expanded bridge must be rejected away from a committed split.");
    require(stats.rejectedOutsideSplitInfluence == 1 &&
            pass.stats.acceptedConnections == 0 &&
            !pass.mask.get(15, 16),
        "A scoped rejection must remove stale path and acceptance statistics.");
}

void testNeighborhoodRescueAlsoRequiresCommittedSplit() {
    const fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask base(grid.nx(), grid.ny());
    fpt::BinaryMask split(grid.nx(), grid.ny());
    auto pass = makeExpandedPass();
    pass.candidates.front().endpointB = {18, 16};
    pass.candidates.front().endpointDistance = 10.0;
    pass.candidates.front().usedNeighborhoodSupport = true;
    const auto stats = fpt::applyScopedWeakBridgePolicy(
        grid, base, split, pass, testOptions());
    require(!pass.candidates.front().accepted &&
            stats.rejectedOutsideSplitInfluence == 1,
        "Neighborhood-only support must remain scoped even inside the baseline distance.");

    split.set(8, 18, true);
    pass = makeExpandedPass();
    pass.candidates.front().endpointB = {18, 16};
    pass.candidates.front().endpointDistance = 10.0;
    pass.candidates.front().usedNeighborhoodSupport = true;
    const auto nearStats = fpt::applyScopedWeakBridgePolicy(
        grid, base, split, pass, testOptions());
    require(pass.candidates.front().accepted &&
            nearStats.acceptedNearSplit == 1,
        "Neighborhood support may be used next to a committed split.");
}

void testPhysicalInfluenceRadiusUsesWorldCoordinates() {
    const fpt::Grid2D grid = makeGrid(4.0, 1.0);
    fpt::BinaryMask base(grid.nx(), grid.ny());
    fpt::BinaryMask split(grid.nx(), grid.ny());
    split.set(9, 16, true);
    auto pass = makeExpandedPass();
    const auto stats = fpt::applyScopedWeakBridgePolicy(
        grid, base, split, pass, testOptions());
    require(stats.rejectedOutsideSplitInfluence == 1,
        "One x sample at four world units must lie outside a three-unit influence radius.");
}

void testDisabledPolicyPreservesConnectorDecision() {
    const fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask base(grid.nx(), grid.ny());
    fpt::BinaryMask split(grid.nx(), grid.ny());
    auto pass = makeExpandedPass();
    auto options = testOptions();
    options.enabled = false;
    const auto beforeDecision = pass.candidates.front().decision;
    const auto stats = fpt::applyScopedWeakBridgePolicy(
        grid, base, split, pass, options);
    require(pass.candidates.front().accepted &&
            pass.candidates.front().decision == beforeDecision &&
            stats.evaluatedScopedCandidates == 0,
        "A disabled scoped policy must preserve the connector result exactly.");

    fpt::WeakContinuityOptions search;
    const int beforeDistance = search.maxDistance;
    fpt::configureScopedWeakBridgeSearch(search, options);
    require(search.maxDistance == beforeDistance &&
            search.supportNeighborhoodRadius == 0,
        "A disabled policy must not alter search options.");
}

} // namespace

int main() {
    testExpandedBridgeNearCommittedSplitIsAccepted();
    testIdenticalBridgeAwayFromSplitIsRejected();
    testNeighborhoodRescueAlsoRequiresCommittedSplit();
    testPhysicalInfluenceRadiusUsesWorldCoordinates();
    testDisabledPolicyPreservesConnectorDecision();
    std::cout << "Scoped weak-bridge policy tests passed.\n";
    return 0;
}
