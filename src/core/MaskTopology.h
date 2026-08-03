#pragma once

#include "BinaryMask.h"

namespace fpt {

struct NewInactiveEnclosureAnalysis {
    BinaryMask cells;
    int componentCount = 0;
    int cellCount = 0;
};

BinaryMask exteriorInactiveCells(const BinaryMask& mask);
int countInactiveHoles(const BinaryMask& mask);
NewInactiveEnclosureAnalysis analyzeNewInactiveEnclosures(
    const BinaryMask& baseMask,
    const BinaryMask& candidateMask);

} // namespace fpt
