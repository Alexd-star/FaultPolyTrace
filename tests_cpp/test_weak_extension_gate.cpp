#include "../src/core/WeakExtensionGate.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(double dx = 1.0, double dy = 1.0) {
    fpt::GridMetadata metadata;
    metadata.nx = 40;
    metadata.ny = 30;
    metadata.dx = dx;
    metadata.dy = dy;
    return fpt::Grid2D(metadata);
}

void testLinearSupportedSingleOwnerComponentPasses() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask strong(grid.nx(), grid.ny());
    std::vector<fpt::SkeletonPoint> cells;
    for (int x = 4; x <= 16; ++x) {
        cells.push_back({x, 12});
        grid.value(x, 12) = x <= 6 ? 0.95 : 0.72;
        if (x <= 6) {
            strong.set(x, 12, true);
        }
    }
    const auto evidence = fpt::evaluateWeakExtensionComponent(
        grid, cells, fpt::buildWeakExtensionOwnerMap(strong),
        0.50, 0.90, fpt::WeakExtensionGateOptions{});
    require(evidence.accepted,
            "A supported linear weak extension touching one seed owner must pass.");
    require(evidence.ownerCount == 1 && evidence.physicalAspect > 1.5,
            "Accepted evidence must retain owner and physical-shape measurements.");
}

void testCompactBlobFailsPhysicalLinearity() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask strong(grid.nx(), grid.ny());
    std::vector<fpt::SkeletonPoint> cells;
    for (int y = 10; y <= 14; ++y) {
        for (int x = 10; x <= 14; ++x) {
            cells.push_back({x, y});
            grid.value(x, y) = 0.85;
        }
    }
    strong.set(10, 12, true);
    const auto evidence = fpt::evaluateWeakExtensionComponent(
        grid, cells, fpt::buildWeakExtensionOwnerMap(strong),
        0.50, 0.90, fpt::WeakExtensionGateOptions{});
    require(!evidence.accepted &&
                evidence.decision == "insufficient_physical_linearity",
            "A compact weak blob must not pass as a fault-like extension.");
}

void testComponentTouchingTwoSeedOwnersIsRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask strong(grid.nx(), grid.ny());
    std::vector<fpt::SkeletonPoint> cells;
    for (int x = 5; x <= 20; ++x) {
        cells.push_back({x, 15});
        grid.value(x, 15) = 0.80;
    }
    strong.set(5, 15, true);
    strong.set(20, 15, true);
    const auto evidence = fpt::evaluateWeakExtensionComponent(
        grid, cells, fpt::buildWeakExtensionOwnerMap(strong),
        0.50, 0.90, fpt::WeakExtensionGateOptions{});
    require(!evidence.accepted && evidence.ownerCount == 2,
            "A weak component competing between two seed owners must be rejected.");
}

void testPhysicalAspectRespondsToAnisotropicSpacing() {
    fpt::Grid2D grid = makeGrid(1.0, 4.0);
    fpt::BinaryMask strong(grid.nx(), grid.ny());
    std::vector<fpt::SkeletonPoint> cells;
    for (int y = 5; y <= 12; ++y) {
        cells.push_back({12, y});
        grid.value(12, y) = 0.80;
    }
    strong.set(12, 5, true);
    const auto evidence = fpt::evaluateWeakExtensionComponent(
        grid, cells, fpt::buildWeakExtensionOwnerMap(strong),
        0.50, 0.90, fpt::WeakExtensionGateOptions{});
    require(evidence.physicalSpan > 20.0,
            "Weak-extension span must use physical y spacing, not index distance.");
}

void testNonFiniteGateOptionsAreRejected() {
    fpt::WeakExtensionGateOptions options;
    options.largeMinAspect = std::numeric_limits<double>::quiet_NaN();
    bool rejected = false;
    try {
        fpt::validateWeakExtensionGateOptions(options);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Non-finite layered weak-extension options must be rejected.");
}

} // namespace

int main() {
    testLinearSupportedSingleOwnerComponentPasses();
    testCompactBlobFailsPhysicalLinearity();
    testComponentTouchingTwoSeedOwnersIsRejected();
    testPhysicalAspectRespondsToAnisotropicSpacing();
    testNonFiniteGateOptionsAreRejected();
    std::cout << "Layered weak-extension gate tests passed.\n";
    return 0;
}
