#pragma once

#include "FaultBeltBuilder.h"
#include "WeakTailRestorer.h"

namespace fpt {

struct WeakTailTopologyGateOptions {
    FaultBeltBuildOptions belt;
    int influencePadding = 2;
};

void applyWeakTailTopologyGate(
    const BinaryMask& baseBranchMask,
    const BinaryMask& observedSkeleton,
    const BinaryMask& attributeMask,
    const BinaryMask& acceptedWeakContinuityMask,
    const WeakTailTopologyGateOptions& options,
    WeakTailResult& tails);

} // namespace fpt
