#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "SkeletonGraph.h"

#include <string>
#include <vector>

namespace fpt {

struct WeakTailOptions {
    int minCells = 6;
    int maxCells = 64;
    double minAspect = 5.0;
    double minMeanSupportRatio = 0.75;
    double minPeakSupportRatio = 0.90;
    double maxAlignmentAngleDegrees = 35.0;
    int ownershipClearanceRadius = 2;
    int tangentRadius = 12;
    bool requireEndpointPortOwnership = false;
    int junctionClearanceRadius = 2;
    int maxEvidenceRecords = 4096;
};

struct WeakTailCandidate {
    int id = 0;
    bool accepted = false;
    std::string decision;
    int cellCount = 0;
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    int ownerLabel = 0;
    int touchedOwnerCount = 0;
    int directOwnerCount = 0;
    int directContactCount = 0;
    int endpointPortCount = 0;
    int endpointX = -1;
    int endpointY = -1;
    bool intersectsJunctionGuard = false;
    double span = 0.0;
    double aspect = 0.0;
    double meanValue = 0.0;
    double maxValue = 0.0;
    double alignmentAngleDegrees = 180.0;
    bool topologyGateEvaluated = false;
    int newEnclosuresBefore = 0;
    int newEnclosuresWithoutCandidate = 0;
    int newEnclosureCellsBefore = 0;
    int newEnclosureCellsWithoutCandidate = 0;
    int topologyTrimmedCells = 0;
    std::vector<SkeletonPoint> cells;
};

struct WeakTailStats {
    int componentCount = 0;
    int sizeRejected = 0;
    int shapeRejected = 0;
    int attributeRejected = 0;
    int ownershipRejected = 0;
    int directContactRejected = 0;
    int endpointPortRejected = 0;
    int junctionGuardRejected = 0;
    int directionRejected = 0;
    int topologyGateEvaluatedComponents = 0;
    int topologyRejectedComponents = 0;
    int topologyFallbackRejectedComponents = 0;
    int topologyRejectedCells = 0;
    int topologyTrimmedComponents = 0;
    int topologyTrimmedCells = 0;
    int topologyInitialNewEnclosures = 0;
    int topologyInitialNewEnclosureCells = 0;
    int topologyRemainingNewEnclosures = 0;
    int topologyRemainingNewEnclosureCells = 0;
    int acceptedComponents = 0;
    int acceptedCells = 0;
};

struct WeakTailResult {
    BinaryMask mask;
    BinaryMask acceptedMask;
    WeakTailStats stats;
    std::vector<WeakTailCandidate> candidates;
};

void validateWeakTailOptions(const WeakTailOptions& options);

WeakTailResult restoreSupportedWeakTails(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    const BinaryMask& keptSkeleton,
    double supportThreshold,
    const WeakTailOptions& options);

} // namespace fpt
