#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"

namespace fpt {

struct SimpleRingCompatibilityOptions {
    int maximumFillCells = 25;
    int maximumOpeningCells = 128;
    int maximumOpeningAttempts = 8;
    double attributeCostWeight = 4.0;
    double strongAttributePenalty = 8.0;
};

struct SimpleRingCompatibilityStats {
    int initialHoleCount = 0;
    int initialHoleCells = 0;
    int filledHoleCount = 0;
    int filledCells = 0;
    int fillRejectedComponents = 0;
    int openedHoleCount = 0;
    int openingCutCells = 0;
    int openingAttributeCells = 0;
    int openingRejectedComponents = 0;
    int remainingHoleCount = 0;
    int remainingHoleCells = 0;
};

struct SimpleRingCompatibilityResult {
    BinaryMask mask;
    SimpleRingCompatibilityStats stats;
};

void validateSimpleRingCompatibilityOptions(
    const SimpleRingCompatibilityOptions& options);

SimpleRingCompatibilityResult enforceSimpleRingCompatibility(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const BinaryMask& attributeMask,
    double referenceThreshold,
    const SimpleRingCompatibilityOptions& options);

} // namespace fpt
