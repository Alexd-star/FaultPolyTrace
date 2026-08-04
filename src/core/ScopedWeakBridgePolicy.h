#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "WeakContinuityConnector.h"

namespace fpt {

struct ScopedWeakBridgeOptions {
    bool enabled = false;
    double baselineMaxDistance = 12.0;
    double expandedMaxDistance = 14.0;
    double splitInfluenceRadiusWorld = 0.0;
    int supportNeighborhoodRadius = 1;
    bool useBoundaryAdjustedCandidateDistance = true;
    bool excludeExistingSkeletonFromSupport = true;
};

struct ScopedWeakBridgePolicyStats {
    int evaluatedScopedCandidates = 0;
    int acceptedNearSplit = 0;
    int rejectedOutsideSplitInfluence = 0;
};

void validateScopedWeakBridgeOptions(const ScopedWeakBridgeOptions& options);

void configureScopedWeakBridgeSearch(
    WeakContinuityOptions& searchOptions,
    const ScopedWeakBridgeOptions& policyOptions);

ScopedWeakBridgePolicyStats applyScopedWeakBridgePolicy(
    const Grid2D& grid,
    const BinaryMask& baseMask,
    const BinaryMask& committedSplitMask,
    WeakContinuityResult& pass,
    const ScopedWeakBridgeOptions& options);

} // namespace fpt
