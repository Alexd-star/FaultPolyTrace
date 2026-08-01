#include "../src/core/AdaptiveBoundary.h"
#include "../src/core/PolygonQualityMetrics.h"

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
    metadata.nx = 20;
    metadata.ny = 20;
    return fpt::Grid2D(metadata);
}

fpt::Polygon makeSquare() {
    fpt::Polygon polygon;
    polygon.id = 1;
    polygon.points = {{5.0, 5.0}, {11.0, 5.0},
                      {11.0, 11.0}, {5.0, 11.0}};
    polygon.kept = true;
    return polygon;
}

void testSupportMaskIsAuthoritative() {
    fpt::Grid2D grid = makeGrid();
    for (double& value : grid.values()) {
        value = 1.0;
    }
    fpt::BinaryMask emptySupport(grid.nx(), grid.ny());
    fpt::AdaptiveBoundaryOptions options;
    options.maximumDisplacementWorld = 2.0;
    options.supportThreshold = 0.5;
    const fpt::AdaptiveBoundaryResult result = fpt::evaluateAdaptiveBoundaries(
        grid, emptySupport, {makeSquare()}, nullptr, options);
    require(result.evidence.size() == 1,
            "One polygon must produce one adaptive-boundary evidence record.");
    require(result.evidence.front().decision ==
                fpt::AdaptiveBoundaryDecision::NoChange,
            "High raw values outside the support mask must not move a boundary.");
}

void testRasterBudgetForcesReviewInsteadOfAcceptance() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask support(grid.nx(), grid.ny());
    for (int y = 3; y <= 13; ++y) {
        for (int x = 2; x <= 4; ++x) {
            support.set(x, y, true);
            grid.value(x, y) = 1.0;
        }
    }
    fpt::AdaptiveBoundaryOptions options;
    options.maximumDisplacementWorld = 2.0;
    options.searchStepWorld = 0.5;
    options.supportThreshold = 0.5;
    options.minimumSupportGain = 0.001;
    options.maximumAreaChangeRatio = 1.0;
    options.maximumRasterCellsPerPolygon = 1;
    const fpt::AdaptiveBoundaryResult result = fpt::evaluateAdaptiveBoundaries(
        grid, support, {makeSquare()}, nullptr, options);
    require(result.evidence.front().candidateChanged,
            "A nearby supported band should create a bounded shadow candidate.");
    require(result.evidence.front().rasterBudgetExceeded,
            "An exhausted raster budget must be reported.");
    require(result.evidence.front().decision ==
                fpt::AdaptiveBoundaryDecision::Review,
            "A budget-limited candidate must never be automatically accepted.");
}

void requireRasterMatchesCellPredicate(
    const fpt::Grid2D& grid,
    const fpt::Polygon& polygon) {
    const fpt::PolygonCellCoverage coverage =
        fpt::rasterizePolygonCellCoverage(grid, polygon, grid.size());
    require(!coverage.budgetExceeded,
            "A full-grid raster budget must cover the complete test polygon.");
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            const bool expected =
                fpt::cellIntersectsPolygonArea(grid, x, y, polygon);
            require(coverage.get(x, y) == expected,
                    "Indexed polygon rasterization must match the reference cell predicate.");
        }
    }
}

void testIndexedRasterizationPreservesCoverageSemantics() {
    fpt::Grid2D grid = makeGrid();
    requireRasterMatchesCellPredicate(grid, makeSquare());

    fpt::Polygon diagonal;
    diagonal.id = 2;
    diagonal.kept = true;
    diagonal.points = {
        {2.0, 3.0}, {16.0, 15.0},
        {15.0, 17.0}, {1.0, 5.0}};
    requireRasterMatchesCellPredicate(grid, diagonal);

    fpt::Polygon withHole = makeSquare();
    withHole.id = 3;
    withHole.holes.push_back({
        {7.0, 7.0}, {9.0, 7.0},
        {9.0, 9.0}, {7.0, 9.0}});
    requireRasterMatchesCellPredicate(grid, withHole);
}

} // namespace

int main() {
    testSupportMaskIsAuthoritative();
    testRasterBudgetForcesReviewInsteadOfAcceptance();
    testIndexedRasterizationPreservesCoverageSemantics();
    std::cout << "Adaptive boundary tests passed.\n";
    return 0;
}
