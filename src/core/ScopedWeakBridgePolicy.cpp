#include "ScopedWeakBridgePolicy.h"

#include "GridMetric.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace fpt {
namespace {

void requireMatchingDimensions(
    const Grid2D& grid,
    const BinaryMask& mask,
    const char* name) {
    if (mask.width() != grid.nx() || mask.height() != grid.ny()) {
        throw std::runtime_error(std::string(name) +
            " dimensions do not match the grid.");
    }
}

} // namespace

void validateScopedWeakBridgeOptions(const ScopedWeakBridgeOptions& options) {
    if (!std::isfinite(options.baselineMaxDistance) ||
        options.baselineMaxDistance < 0.0 ||
        !std::isfinite(options.expandedMaxDistance) ||
        options.expandedMaxDistance < options.baselineMaxDistance) {
        throw std::runtime_error(
            "Scoped weak-bridge distances are invalid.");
    }
    if (!std::isfinite(options.splitInfluenceRadiusWorld) ||
        options.splitInfluenceRadiusWorld < 0.0) {
        throw std::runtime_error(
            "Scoped weak-bridge influence radius is invalid.");
    }
    if (options.supportNeighborhoodRadius < 0) {
        throw std::runtime_error(
            "Scoped weak-bridge support radius must be non-negative.");
    }
}

void configureScopedWeakBridgeSearch(
    WeakContinuityOptions& searchOptions,
    const ScopedWeakBridgeOptions& policyOptions) {
    validateScopedWeakBridgeOptions(policyOptions);
    if (!policyOptions.enabled) {
        return;
    }
    searchOptions.maxDistance = static_cast<int>(
        std::floor(policyOptions.expandedMaxDistance + 1e-9));
    searchOptions.useBoundaryAdjustedCandidateDistance =
        policyOptions.useBoundaryAdjustedCandidateDistance;
    searchOptions.supportNeighborhoodRadius =
        policyOptions.supportNeighborhoodRadius;
    searchOptions.excludeExistingSkeletonFromSupport =
        policyOptions.excludeExistingSkeletonFromSupport;
}

ScopedWeakBridgePolicyStats applyScopedWeakBridgePolicy(
    const Grid2D& grid,
    const BinaryMask& baseMask,
    const BinaryMask& committedSplitMask,
    WeakContinuityResult& pass,
    const ScopedWeakBridgeOptions& options) {
    validateScopedWeakBridgeOptions(options);
    requireMatchingDimensions(grid, baseMask, "Base mask");
    requireMatchingDimensions(grid, committedSplitMask,
        "Committed split mask");
    ScopedWeakBridgePolicyStats stats;
    if (!options.enabled) {
        return stats;
    }

    const GridMetric metric(grid);
    const std::vector<GridOffset> influenceOffsets =
        metric.offsetsWithinRadius(options.splitInfluenceRadiusWorld);
    const auto nearCommittedSplit = [&](const SkeletonPoint& endpoint) {
        for (const GridOffset& offset : influenceOffsets) {
            if (committedSplitMask.get(
                    endpoint.x + offset.x, endpoint.y + offset.y)) {
                return true;
            }
        }
        return false;
    };

    for (WeakContinuityCandidate& candidate : pass.candidates) {
        if (!candidate.accepted) {
            continue;
        }
        const bool usesExpandedDistance =
            candidate.endpointDistance > options.baselineMaxDistance;
        const bool usesScopedNeighborhoodEvidence =
            candidate.usedNeighborhoodSupport;
        if (!usesExpandedDistance && !usesScopedNeighborhoodEvidence) {
            continue;
        }
        ++stats.evaluatedScopedCandidates;
        if (nearCommittedSplit(candidate.endpointA) ||
            nearCommittedSplit(candidate.endpointB)) {
            ++stats.acceptedNearSplit;
            continue;
        }
        candidate.accepted = false;
        candidate.selectedByGlobalMatching = false;
        candidate.decision = "outside_local_split_influence";
        ++stats.rejectedOutsideSplitInfluence;
        ++pass.stats.competitionRejected;
    }

    pass.mask = baseMask;
    pass.stats.acceptedConnections = 0;
    pass.stats.addedPathPixels = 0;
    for (const WeakContinuityCandidate& candidate : pass.candidates) {
        if (!candidate.accepted) {
            continue;
        }
        ++pass.stats.acceptedConnections;
        pass.stats.addedPathPixels += candidate.addedPathPixels;
        for (const SkeletonPoint& point : candidate.path) {
            pass.mask.set(point.x, point.y, true);
        }
    }
    return stats;
}

} // namespace fpt
