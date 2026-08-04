#include "../src/core/SupportEnvelopeSeparator.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(
    int nx = 64,
    int ny = 56,
    double dx = 1.0,
    double dy = 1.0) {
    fpt::GridMetadata metadata;
    metadata.nx = nx;
    metadata.ny = ny;
    metadata.dx = dx;
    metadata.dy = dy;
    return fpt::Grid2D(metadata);
}

fpt::SupportEnvelopeSeparatorOptions testOptions() {
    fpt::SupportEnvelopeSeparatorOptions options;
    options.enabled = true;
    options.lowThreshold = 0.20;
    options.supportRadiusWorld = 2.0;
    options.minPartCells = 10;
    options.minPartRatio = 0.03;
    options.minRemovedCells = 2;
    options.minLowRatio = 0.0;
    options.maxRemovedRatio = 0.5;
    return options;
}

void fillActiveRectangle(
    fpt::BinaryMask& active,
    int minX,
    int minY,
    int maxX,
    int maxY) {
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            active.set(x, y, true);
        }
    }
}

void drawLine(
    fpt::Grid2D& grid,
    int x0,
    int y0,
    int x1,
    int y1,
    double value = 0.9) {
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        grid.value(x0, y0) = value;
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twiceError = error * 2;
        if (twiceError >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twiceError <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void testParallelSupportedTracesAreSeparated() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillActiveRectangle(active, 8, 10, 52, 24);
    for (int y : {12, 22}) {
        for (int x = 10; x <= 50; ++x) {
            grid.value(x, y) = 0.90;
        }
    }
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, testOptions());
    require(result.stats.acceptedComponents == 1,
        "Two parallel supported traces inside one unsupported fill must split.");
    require(result.committedCutMask.countActive() > 0,
        "An accepted separation must expose its committed cut mask.");
}

void testDiagonalParallelTracesAreSeparatedWithoutAxisBias() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask support(grid.nx(), grid.ny());
    drawLine(grid, 8, 10, 46, 34);
    drawLine(grid, 8, 17, 46, 41);
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (grid.value(x, y) > 0.8) {
                support.set(x, y, true);
            }
        }
    }
    const fpt::BinaryMask active = fpt::dilate(support, 3);
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, testOptions());
    require(result.stats.acceptedComponents == 1,
        "Diagonal parallel traces must be separated without horizontal or vertical bias.");
}

void testWideFaultBeltRemainsOneObject() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillActiveRectangle(active, 8, 10, 52, 24);
    for (int y = 15; y <= 19; ++y) {
        for (int x = 10; x <= 50; ++x) {
            grid.value(x, y) = 0.90;
        }
    }
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, testOptions());
    require(result.stats.acceptedComponents == 0,
        "A single wide supported fault belt must not be split.");
}

void testYBranchAndCrossingRemainConnected() {
    for (int mode = 0; mode < 2; ++mode) {
        fpt::Grid2D grid = makeGrid();
        fpt::BinaryMask active(grid.nx(), grid.ny());
        fillActiveRectangle(active, 8, 6, 54, 48);
        if (mode == 0) {
            drawLine(grid, 30, 42, 30, 24);
            drawLine(grid, 30, 24, 16, 10);
            drawLine(grid, 30, 24, 46, 10);
        } else {
            drawLine(grid, 12, 12, 50, 44);
            drawLine(grid, 12, 44, 50, 12);
        }
        auto options = testOptions();
        options.maxRemovedRatio = 0.5;
        const auto result = fpt::separateUnsupportedEnvelopes(
            grid, active, options);
        require(result.stats.acceptedComponents == 0,
            "Connected Y or crossing support must remain one topological object.");
    }
}

void testSupportedWeakBridgeIsNotCut() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillActiveRectangle(active, 8, 16, 52, 26);
    drawLine(grid, 10, 21, 50, 21, 0.35);
    for (int x = 10; x <= 20; ++x) {
        grid.value(x, 21) = 0.90;
    }
    for (int x = 40; x <= 50; ++x) {
        grid.value(x, 21) = 0.90;
    }
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, testOptions());
    require(result.stats.acceptedComponents == 0,
        "A weak bridge above the support threshold must not be removed.");
}

void testSmallNoiseAndThreeWaySplitAreRejected() {
    {
        fpt::Grid2D grid = makeGrid();
        fpt::BinaryMask active(grid.nx(), grid.ny());
        fillActiveRectangle(active, 2, 2, 3, 3);
        const auto result = fpt::separateUnsupportedEnvelopes(
            grid, active, testOptions());
        require(result.committedCutMask.countActive() == 0,
            "Small noise components must be ignored.");
    }
    {
        fpt::Grid2D grid = makeGrid();
        fpt::BinaryMask active(grid.nx(), grid.ny());
        fillActiveRectangle(active, 8, 8, 52, 30);
        for (int y : {10, 19, 28}) {
            for (int x = 10; x <= 50; ++x) {
                grid.value(x, y) = 0.90;
            }
        }
        const auto result = fpt::separateUnsupportedEnvelopes(
            grid, active, testOptions());
        require(result.stats.acceptedComponents == 0,
            "A three-way split must remain review-only.");
        bool ambiguous = false;
        for (const auto& decision : result.decisions) {
            ambiguous = ambiguous ||
                decision.decision == "ambiguous_multi_part_split" ||
                decision.decision == "ambiguous_support_component_count";
        }
        require(ambiguous,
            "A three-way split must retain an explicit ambiguity reason.");
    }
}

void testCompactSupportedBlobsRemainReviewOnly() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillActiveRectangle(active, 5, 13, 31, 21);
    for (int y = 15; y <= 19; ++y) {
        for (int x = 7; x <= 11; ++x) {
            grid.value(x, y) = 0.90;
        }
        for (int x = 25; x <= 29; ++x) {
            grid.value(x, y) = 0.90;
        }
    }
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, testOptions());
    require(result.stats.acceptedComponents == 0,
        "Two compact supported blobs must not be treated as fault traces.");
    bool rejectedLinearity = false;
    for (const auto& decision : result.decisions) {
        rejectedLinearity = rejectedLinearity ||
            decision.decision == "insufficient_part_linearity";
    }
    require(rejectedLinearity,
        "Compact supported blobs must retain a physical-linearity rejection.");
}

void testPhysicalRadiusHonorsNonSquareSampling() {
    fpt::Grid2D grid = makeGrid(11, 11, 4.0, 1.0);
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillActiveRectangle(active, 1, 1, 9, 9);
    grid.value(5, 5) = 0.90;
    auto options = testOptions();
    options.supportRadiusWorld = 2.0;
    options.minPartCells = 1;
    options.minRemovedCells = 1;
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, options);
    require(result.candidateCutMask.get(6, 5),
        "One x sample at four world units must be outside a two-unit radius.");
    require(!result.candidateCutMask.get(5, 7),
        "Two y samples at two world units must remain inside the radius.");
}

void testProtectedWeakPathPreventsInvalidCommit() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fpt::BinaryMask protectedMask(grid.nx(), grid.ny());
    fillActiveRectangle(active, 8, 10, 52, 24);
    for (int y : {12, 22}) {
        for (int x = 10; x <= 50; ++x) {
            grid.value(x, y) = 0.90;
        }
    }
    for (int y = 13; y <= 21; ++y) {
        protectedMask.set(30, y, true);
    }
    const auto result = fpt::separateUnsupportedEnvelopes(
        grid, active, testOptions(), &protectedMask);
    require(result.stats.acceptedComponents == 0,
        "A protected accepted weak path must prevent a contradictory split.");
    require(result.committedCutMask.countActive() == 0,
        "Protected weak-path cells must never appear in a committed cut.");
}

} // namespace

int main() {
    testParallelSupportedTracesAreSeparated();
    testDiagonalParallelTracesAreSeparatedWithoutAxisBias();
    testWideFaultBeltRemainsOneObject();
    testYBranchAndCrossingRemainConnected();
    testSupportedWeakBridgeIsNotCut();
    testSmallNoiseAndThreeWaySplitAreRejected();
    testPhysicalRadiusHonorsNonSquareSampling();
    testProtectedWeakPathPreventsInvalidCommit();
    testCompactSupportedBlobsRemainReviewOnly();
    std::cout << "Support-envelope separator tests passed.\n";
    return 0;
}
