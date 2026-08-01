#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "SkeletonGraph.h"
#include "WeakContinuityConnector.h"

namespace fpt {

struct FinalBridgeGateOptions {
    double supportThreshold = 0.0;
    int junctionInfluenceDistance = 0;
    int maximumBeltRadius = 1;
    int supportEnvelopeRadius = 0;
};

struct FinalBridgeGateTimings {
    double contextBuildMs = 0.0;
    double ownershipInitializationMs = 0.0;
    double componentHoleCheckMs = 0.0;
    double envelopeBuildMs = 0.0;
    double localTopologyCheckMs = 0.0;
    double totalMs = 0.0;
};

struct FinalBridgeGateResult {
    BinaryMask mask;
    int acceptedConnections = 0;
    int rejectedConnections = 0;
    int addedEnvelopeCells = 0;
    FinalBridgeGateTimings timings;
};

FinalBridgeGateResult applyFinalObjectBridgeGates(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const SkeletonGraph& finalGraph,
    WeakContinuityResult& pass,
    const FinalBridgeGateOptions& options);

} // namespace fpt
