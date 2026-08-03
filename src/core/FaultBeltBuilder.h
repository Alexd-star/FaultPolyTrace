#pragma once

#include "BinaryMask.h"

namespace fpt {

struct FaultBeltBuildOptions {
    int beltRadius = 0;
    bool attributeConstrained = false;
    int attributeEnvelopeRadius = 0;
    int closeRadius = 0;
    bool preserveObservedSkeletonEnvelope = false;
};

BinaryMask buildFaultBelt(
    const BinaryMask& branchMask,
    const BinaryMask& observedSkeleton,
    const BinaryMask& attributeMask,
    const BinaryMask& acceptedWeakContinuityMask,
    const BinaryMask& acceptedWeakTailMask,
    const FaultBeltBuildOptions& options);

} // namespace fpt
