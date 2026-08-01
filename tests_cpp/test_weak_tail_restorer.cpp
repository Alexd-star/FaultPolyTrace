#include "../src/core/WeakTailRestorer.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(int width = 48, int height = 32) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    return fpt::Grid2D(metadata);
}

void setHorizontal(
    fpt::BinaryMask& mask, fpt::Grid2D& grid,
    int x0, int x1, int y, double value) {
    for (int x = x0; x <= x1; ++x) {
        mask.set(x, y, true);
        grid.value(x, y) = value;
    }
}

void testAlignedSupportedTailIsRestored() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask kept(grid.nx(), grid.ny());
    setHorizontal(skeleton, grid, 4, 30, 14, 0.82);
    setHorizontal(kept, grid, 4, 20, 14, 0.96);
    grid.value(21, 14) = 0.92;
    fpt::WeakTailResult result = fpt::restoreSupportedWeakTails(
        grid, skeleton, kept, 0.95, fpt::WeakTailOptions{});
    require(result.stats.acceptedComponents == 1,
            "Aligned supported endpoint chain should be restored.");
    require(result.mask.get(30, 14), "Restored mask must include the tail endpoint.");
}

void testPerpendicularSpurIsRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask kept(grid.nx(), grid.ny());
    setHorizontal(skeleton, grid, 4, 24, 14, 0.96);
    setHorizontal(kept, grid, 4, 24, 14, 0.96);
    for (int y = 15; y <= 24; ++y) {
        skeleton.set(14, y, true);
        grid.value(14, y) = 0.88;
    }
    fpt::WeakTailResult result = fpt::restoreSupportedWeakTails(
        grid, skeleton, kept, 0.95, fpt::WeakTailOptions{});
    require(result.stats.acceptedComponents == 0,
            "Perpendicular side spur must not be restored as an endpoint continuation.");
    require(!result.mask.get(14, 24), "Rejected spur must stay outside the result mask.");
}

void testBridgeBetweenOwnersIsRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask kept(grid.nx(), grid.ny());
    setHorizontal(skeleton, grid, 3, 40, 16, 0.86);
    setHorizontal(kept, grid, 3, 14, 16, 0.96);
    setHorizontal(kept, grid, 29, 40, 16, 0.96);
    fpt::WeakTailResult result = fpt::restoreSupportedWeakTails(
        grid, skeleton, kept, 0.95, fpt::WeakTailOptions{});
    require(result.stats.acceptedComponents == 0,
            "A chain touching two existing owners must remain a review candidate.");
    require(!result.mask.get(21, 16), "Ambiguous bridge must not enter the result mask.");
}

void testStrictEndpointPortTailIsRestored() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask kept(grid.nx(), grid.ny());
    setHorizontal(skeleton, grid, 4, 30, 14, 0.88);
    setHorizontal(kept, grid, 4, 20, 14, 0.96);
    fpt::WeakTailOptions options;
    options.requireEndpointPortOwnership = true;
    const fpt::WeakTailResult result = fpt::restoreSupportedWeakTails(
        grid, skeleton, kept, 0.95, options);
    require(result.stats.acceptedComponents == 1,
            "A directly attached, aligned, uniquely owned endpoint tail must pass strict mode.");
    require(!result.candidates.empty() &&
                result.candidates.front().endpointPortCount == 1 &&
                result.candidates.front().directOwnerCount == 1,
            "Strict tail evidence must record its unique endpoint port and direct owner.");
}

void testStrictModeRejectsNearButDetachedTail() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask kept(grid.nx(), grid.ny());
    setHorizontal(kept, grid, 4, 20, 14, 0.96);
    setHorizontal(skeleton, grid, 4, 20, 14, 0.96);
    setHorizontal(skeleton, grid, 22, 30, 14, 0.90);
    fpt::WeakTailOptions options;
    options.requireEndpointPortOwnership = true;
    const fpt::WeakTailResult result = fpt::restoreSupportedWeakTails(
        grid, skeleton, kept, 0.95, options);
    require(result.stats.acceptedComponents == 0,
            "A tail merely near an owner must not pass strict direct-contact ownership.");
    require(result.stats.directContactRejected == 1,
            "The detached-tail rejection reason must be explicit.");
}

void testStrictModeRejectsInteriorSpurBeforeDirectionHeuristic() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::BinaryMask kept(grid.nx(), grid.ny());
    setHorizontal(skeleton, grid, 4, 24, 14, 0.96);
    setHorizontal(kept, grid, 4, 24, 14, 0.96);
    for (int y = 15; y <= 24; ++y) {
        skeleton.set(14, y, true);
        grid.value(14, y) = 0.90;
    }
    fpt::WeakTailOptions options;
    options.requireEndpointPortOwnership = true;
    const fpt::WeakTailResult result = fpt::restoreSupportedWeakTails(
        grid, skeleton, kept, 0.95, options);
    require(result.stats.endpointPortRejected == 1 ||
                result.stats.junctionGuardRejected == 1,
            "An interior spur must fail endpoint-port or junction protection in strict mode.");
}

void testNonFiniteTailOptionsAreRejected() {
    fpt::WeakTailOptions options;
    options.minAspect = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try {
        fpt::validateWeakTailOptions(options);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Non-finite weak-tail options must be rejected.");
}

} // namespace

int main() {
    try {
        testAlignedSupportedTailIsRestored();
        testPerpendicularSpurIsRejected();
        testBridgeBetweenOwnersIsRejected();
        testStrictEndpointPortTailIsRestored();
        testStrictModeRejectsNearButDetachedTail();
        testStrictModeRejectsInteriorSpurBeforeDirectionHeuristic();
        testNonFiniteTailOptionsAreRejected();
        std::cout << "Weak tail restorer tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
