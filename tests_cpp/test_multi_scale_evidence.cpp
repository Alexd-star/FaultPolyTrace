#include "../src/core/MultiScaleEvidence.h"

#include <cmath>
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
    metadata.nx = 25;
    metadata.ny = 25;
    metadata.dx = 10.0;
    metadata.dy = 20.0;
    return fpt::Grid2D(metadata);
}

void testOppositeSkeletonNeighborsDoNotCancelDirection() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask skeleton(grid.nx(), grid.ny());
    fpt::MultiScaleEvidenceResult evidence;
    for (int x = 5; x <= 19; ++x) {
        skeleton.set(x, 12, true);
        fpt::MultiScaleEvidenceSample sample;
        sample.x = x;
        sample.y = 12;
        sample.orientationX = 1.0;
        sample.orientationY = 0.0;
        sample.finite = true;
        sample.confident = true;
        evidence.samples.push_back(sample);
    }

    fpt::evaluateMultiScaleEvidenceAgainstSkeleton(
        grid, skeleton, 5.0, evidence);
    require(evidence.summary.skeletonCellsEvaluated >= 11,
            "A straight skeleton must retain a PCA direction despite opposite neighbors.");
    require(evidence.summary.skeletonOrientationAgreementCells ==
                evidence.summary.skeletonCellsEvaluated,
            "Horizontal auxiliary evidence must agree with a horizontal skeleton.");
    require(evidence.summary.meanSkeletonOrientationAngle < 1e-6,
            "The mean undirected orientation error should be zero on a straight line.");
}

void testBrightRidgeProducesFiniteAuxiliaryEvidence() {
    fpt::Grid2D grid = makeGrid();
    fpt::BinaryMask target(grid.nx(), grid.ny());
    for (int x = 3; x < grid.nx() - 3; ++x) {
        grid.value(x, 12) = 1.0;
        grid.value(x, 11) = 0.35;
        grid.value(x, 13) = 0.35;
        target.set(x, 12, true);
    }
    fpt::MultiScaleEvidenceOptions options;
    options.scalesWorld = {10.0, 20.0};
    options.minimumConfidence = 0.0;
    options.minimumRidgeResponse = 0.0;
    const fpt::MultiScaleEvidenceResult evidence =
        fpt::computeMultiScaleEvidence(grid, &target, options);
    require(evidence.summary.evaluatedCells == target.countActive(),
            "Every target ridge cell must be evaluated within the sample budget.");
    require(evidence.summary.finiteCells > 0,
            "A finite bright ridge must produce finite multi-scale evidence.");
    require(evidence.summary.maximumRidgeResponse > 0.0,
            "A bright line must have a positive ridge response.");
}

} // namespace

int main() {
    testOppositeSkeletonNeighborsDoNotCancelDirection();
    testBrightRidgeProducesFiniteAuxiliaryEvidence();
    std::cout << "Multi-scale evidence tests passed.\n";
    return 0;
}
