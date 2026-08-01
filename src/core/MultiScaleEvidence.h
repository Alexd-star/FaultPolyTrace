#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"

#include <cstddef>
#include <vector>

namespace fpt {

// Auxiliary, read-only evidence derived from the supplied attribute grid.
// The original attribute remains the primary decision signal.
struct MultiScaleEvidenceOptions {
    std::vector<double> scalesWorld;
    double minimumConfidence = 0.18;
    double minimumRidgeResponse = 0.04;
    std::size_t maximumSamples = 2000000;
};

struct MultiScaleEvidenceSample {
    int x = 0;
    int y = 0;
    double scaleWorld = 0.0;
    double ridgeResponse = 0.0;
    double contrastResponse = 0.0;
    double confidence = 0.0;
    double orientationX = 1.0;
    double orientationY = 0.0;
    bool finite = false;
    bool confident = false;
};

struct MultiScaleEvidenceSummary {
    int targetCells = 0;
    int evaluatedCells = 0;
    int finiteCells = 0;
    int confidentCells = 0;
    int truncatedCells = 0;
    int invalidNeighborhoods = 0;
    double meanRidgeResponse = 0.0;
    double meanConfidence = 0.0;
    double maximumRidgeResponse = 0.0;
    int skeletonCellsEvaluated = 0;
    int skeletonOrientationAgreementCells = 0;
    double meanSkeletonOrientationAngle = 180.0;
};

struct MultiScaleEvidenceResult {
    MultiScaleEvidenceSummary summary;
    std::vector<MultiScaleEvidenceSample> samples;
};

void validateMultiScaleEvidenceOptions(const MultiScaleEvidenceOptions& options);

MultiScaleEvidenceResult computeMultiScaleEvidence(
    const Grid2D& grid,
    const BinaryMask* targetMask,
    const MultiScaleEvidenceOptions& options = MultiScaleEvidenceOptions{});

// Compares the auxiliary tangent orientation with the local skeleton direction.
// This is a diagnostic only; it never changes the skeleton or polygon output.
void evaluateMultiScaleEvidenceAgainstSkeleton(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    double maximumAgreementAngleDegrees,
    MultiScaleEvidenceResult& result);

} // namespace fpt
