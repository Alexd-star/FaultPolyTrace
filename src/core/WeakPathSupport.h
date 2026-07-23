#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"

#include <limits>

namespace fpt {

struct WeakPathSupportOptions {
    double supportThreshold = 0.0;
    int maxUnsupportedRun = 3;
    int seedSearchRadius = 0;
    int minEvidenceCells = 0;
    double minMeanAttribute = -std::numeric_limits<double>::infinity();
    double lowerQuantile = 0.10;
    double minLowerQuantileAttribute = -std::numeric_limits<double>::infinity();
    double minAspectRatio = 0.0;
    double maxEstimatedWidth = std::numeric_limits<double>::infinity();
    double maxTurnAngleDegrees = 180.0;
    int directionWindow = 2;
};

struct WeakPathSupportStats {
    int candidateCellCount = 0;
    int seedCellCount = 0;
    int seedContactCellCount = 0;
    int acceptedCellCount = 0;
    int rejectedCellCount = 0;
    int rootConflictRejectedCellCount = 0;
    int maxAcceptedUnsupportedRun = 0;
    int candidateComponentCount = 0;
    int acceptedComponentCount = 0;
    int rejectedComponentCount = 0;
    int partiallyRejectedComponentCount = 0;
    int evidenceEvaluatedFragmentCount = 0;
    int attributeRejectedFragmentCount = 0;
    int shapeRejectedFragmentCount = 0;
    int directionRejectedFragmentCount = 0;
    bool insufficientSeedEvidence = false;
    double meanAttribute = 0.0;
    double lowerQuantileAttribute = 0.0;
    double directionX = 1.0;
    double directionY = 0.0;
    double majorAxisLength = 0.0;
    double estimatedWidth = 0.0;
    double aspectRatio = 1.0;
    double maxObservedComponentWidth = 0.0;
    double maxObservedTurnAngleDegrees = 0.0;
};

struct WeakPathSupportResult {
    BinaryMask acceptedMask;
    WeakPathSupportStats stats;
};

WeakPathSupportResult selectWeakPathSupport(
    const Grid2D& grid,
    const BinaryMask& candidateMask,
    const BinaryMask& seedMask,
    const WeakPathSupportOptions& options);

} // namespace fpt
