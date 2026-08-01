#include "../src/core/GridMetric.h"
#include "../src/core/PhysicalMorphology.h"

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

fpt::GridMetadata makeMetadata(double dx, double dy, int nx = 9, int ny = 9) {
    fpt::GridMetadata metadata;
    metadata.nx = nx;
    metadata.ny = ny;
    metadata.dx = dx;
    metadata.dy = dy;
    return metadata;
}

void testPhysicalDistanceAndDirectionUseBothSpacings() {
    const fpt::GridMetric metric(makeMetadata(10.0, 20.0));
    require(std::abs(metric.distance(0.0, 0.0, 2.0, 0.0) - 20.0) < 1e-9,
            "Two x samples must equal 20 physical units.");
    require(std::abs(metric.distance(0.0, 0.0, 0.0, 1.0) - 20.0) < 1e-9,
            "One y sample must equal 20 physical units.");
    require(std::abs(metric.directionDegrees(0.0, 0.0, 2.0, 1.0) - 45.0) < 1e-9,
            "Direction must be measured after converting both axes to physical units.");
    require(!metric.isIsotropic(), "A 10 by 20 grid is not physically isotropic.");
}

void testEllipticalOffsetsRespectPhysicalRadius() {
    const fpt::GridMetric metric(makeMetadata(10.0, 20.0));
    const std::vector<fpt::GridOffset> offsets = metric.offsetsWithinRadius(20.0);
    require(offsets.size() == 7,
            "A 20-unit disk on a 10 by 20 grid must contain seven sample centers.");
    bool hasDiagonal = false;
    bool hasTwoX = false;
    bool hasOneY = false;
    for (const fpt::GridOffset& offset : offsets) {
        hasDiagonal = hasDiagonal || (std::abs(offset.x) == 1 && std::abs(offset.y) == 1);
        hasTwoX = hasTwoX || (std::abs(offset.x) == 2 && offset.y == 0);
        hasOneY = hasOneY || (offset.x == 0 && std::abs(offset.y) == 1);
    }
    require(!hasDiagonal, "A 10 by 20 diagonal lies outside a 20-unit disk.");
    require(hasTwoX && hasOneY, "Physical axial offsets at exactly the radius must be included.");
}

void testPhysicalDilationUsesDiskInsteadOfSquare() {
    const fpt::GridMetric metric(makeMetadata(10.0, 10.0));
    fpt::BinaryMask mask(metric.nx(), metric.ny());
    mask.set(4, 4, true);
    const fpt::BinaryMask dilated = fpt::dilatePhysical(mask, metric, 10.0);
    require(dilated.countActive() == 5,
            "A one-sample physical disk must produce a cross, not a 3 by 3 square.");
    require(!dilated.get(3, 3), "Diagonal cells must remain outside the circular element.");
}

void testInvalidMetricAndResourceBudgetAreRejected() {
    bool spacingRejected = false;
    try {
        fpt::GridMetric invalid(makeMetadata(
            std::numeric_limits<double>::quiet_NaN(), 10.0));
    } catch (const std::runtime_error&) {
        spacingRejected = true;
    }
    require(spacingRejected, "Non-finite grid spacing must be rejected.");

    bool budgetRejected = false;
    try {
        const fpt::GridMetric metric(makeMetadata(1.0, 1.0));
        (void)metric.offsetsWithinRadius(100.0, true, 100);
    } catch (const std::runtime_error&) {
        budgetRejected = true;
    }
    require(budgetRejected, "Excessive physical structuring elements must respect the offset budget.");
}

} // namespace

int main() {
    testPhysicalDistanceAndDirectionUseBothSpacings();
    testEllipticalOffsetsRespectPhysicalRadius();
    testPhysicalDilationUsesDiskInsteadOfSquare();
    testInvalidMetricAndResourceBudgetAreRejected();
    std::cout << "Grid metric and physical morphology tests passed.\n";
    return 0;
}
