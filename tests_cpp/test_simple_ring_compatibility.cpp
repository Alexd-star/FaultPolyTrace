#include "../src/core/MaskTopology.h"
#include "../src/core/SimpleRingCompatibility.h"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(int width, int height) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    metadata.sx = 0.0;
    metadata.sy = 0.0;
    metadata.dx = 1.0;
    metadata.dy = 1.0;
    fpt::Grid2D grid(metadata);
    grid.values().assign(static_cast<std::size_t>(width * height), 0.2);
    return grid;
}

void testSmallHoleIsFilledWithoutChangingObjectCount() {
    fpt::Grid2D grid = makeGrid(20, 16);
    fpt::BinaryMask mask(20, 16);
    for (int y = 4; y <= 10; ++y) {
        for (int x = 4; x <= 14; ++x) {
            mask.set(x, y, true);
        }
    }
    mask.set(9, 7, false);
    fpt::SimpleRingCompatibilityOptions options;
    const auto result = fpt::enforceSimpleRingCompatibility(
        grid, mask, mask, 0.8, options);
    require(result.stats.initialHoleCount == 1 &&
                result.stats.filledHoleCount == 1 &&
                result.stats.filledCells == 1,
            "A one-cell interior raster void must be filled.");
    require(result.stats.remainingHoleCount == 0 && result.mask.get(9, 7),
            "The filled result must contain no interior ring.");
}

void testLargeHoleUsesNarrowLowSupportOpening() {
    fpt::Grid2D grid = makeGrid(26, 22);
    fpt::BinaryMask mask(26, 22);
    for (int y = 3; y <= 18; ++y) {
        for (int x = 3; x <= 22; ++x) {
            if (x <= 5 || x >= 20 || y <= 5 || y >= 16) {
                mask.set(x, y, true);
                grid.value(x, y) = 0.95;
            }
        }
    }
    for (int x = 3; x <= 5; ++x) {
        grid.value(x, 11) = 0.1;
    }
    fpt::BinaryMask attributes(26, 22);
    for (int y = 0; y < 22; ++y) {
        for (int x = 0; x < 26; ++x) {
            if (mask.get(x, y) && grid.value(x, y) >= 0.8) {
                attributes.set(x, y, true);
            }
        }
    }
    fpt::SimpleRingCompatibilityOptions options;
    options.maximumFillCells = 4;
    const auto result = fpt::enforceSimpleRingCompatibility(
        grid, mask, attributes, 0.8, options);
    require(result.stats.openedHoleCount == 1 &&
                result.stats.remainingHoleCount == 0,
            "A large background loop must be opened instead of filled.");
    require(result.stats.openingCutCells == 3 &&
                !result.mask.get(3, 11) &&
                !result.mask.get(4, 11) &&
                !result.mask.get(5, 11),
            "The opening must cross the narrow low-support side of the ring.");
}

void testAlreadySimpleMaskIsUnchanged() {
    fpt::Grid2D grid = makeGrid(16, 12);
    fpt::BinaryMask mask(16, 12);
    for (int x = 2; x <= 12; ++x) {
        mask.set(x, 6, true);
    }
    const auto result = fpt::enforceSimpleRingCompatibility(
        grid, mask, mask, 0.8, {});
    require(result.stats.initialHoleCount == 0 &&
                result.stats.remainingHoleCount == 0 &&
                result.mask.values() == mask.values(),
            "A hole-free mask must remain byte-for-byte unchanged.");
}

} // namespace

int main() {
    try {
        testSmallHoleIsFilledWithoutChangingObjectCount();
        testLargeHoleUsesNarrowLowSupportOpening();
        testAlreadySimpleMaskIsUnchanged();
        std::cout << "Simple-ring compatibility tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
