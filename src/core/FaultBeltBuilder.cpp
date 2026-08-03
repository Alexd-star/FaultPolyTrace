#include "FaultBeltBuilder.h"

#include <stdexcept>
#include <string>

namespace fpt {
namespace {

void requireMatchingMask(
    const BinaryMask& reference,
    const BinaryMask& candidate,
    const char* name) {
    if (reference.width() != candidate.width() ||
        reference.height() != candidate.height()) {
        throw std::runtime_error(
            std::string("Fault belt mask dimensions do not match: ") + name);
    }
}

} // namespace

BinaryMask buildFaultBelt(
    const BinaryMask& branchMask,
    const BinaryMask& observedSkeleton,
    const BinaryMask& attributeMask,
    const BinaryMask& acceptedWeakContinuityMask,
    const BinaryMask& acceptedWeakTailMask,
    const FaultBeltBuildOptions& options) {
    if (options.beltRadius < 0 || options.attributeEnvelopeRadius < 0 ||
        options.closeRadius < 0) {
        throw std::runtime_error("Fault belt radii must be non-negative.");
    }
    requireMatchingMask(branchMask, observedSkeleton, "observed skeleton");
    requireMatchingMask(branchMask, attributeMask, "attribute mask");
    requireMatchingMask(
        branchMask, acceptedWeakContinuityMask, "weak continuity mask");
    requireMatchingMask(branchMask, acceptedWeakTailMask, "weak tail mask");

    BinaryMask belt = bufferMask(branchMask, options.beltRadius);
    if (options.attributeConstrained) {
        BinaryMask envelopeSeed = attributeMask;
        std::vector<unsigned char>& seedValues = envelopeSeed.values();
        const std::vector<unsigned char>& branchValues = branchMask.values();
        const std::vector<unsigned char>& skeletonValues =
            observedSkeleton.values();
        const std::vector<unsigned char>& continuityValues =
            acceptedWeakContinuityMask.values();
        const std::vector<unsigned char>& tailValues =
            acceptedWeakTailMask.values();
        for (std::size_t index = 0; index < seedValues.size(); ++index) {
            const bool observedBranch =
                options.preserveObservedSkeletonEnvelope &&
                branchValues[index] && skeletonValues[index];
            if (observedBranch || continuityValues[index] || tailValues[index]) {
                seedValues[index] = 1;
            }
        }
        const BinaryMask envelope = options.attributeEnvelopeRadius > 0
            ? dilate(envelopeSeed, options.attributeEnvelopeRadius)
            : envelopeSeed;
        belt = intersectMask(belt, envelope);
    }
    if (options.closeRadius > 0) {
        belt = closeMask(belt, options.closeRadius);
    }
    return belt;
}

} // namespace fpt
