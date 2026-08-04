#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "SkeletonGraph.h"

#include <string>
#include <vector>

namespace fpt {

struct WeakContinuityOptions {
    int maxDistance = 42;
    int tangentWindow = 8;
    int corridorRadius = 8;
    int maxCandidatesPerEndpoint = 4;
    double maxEndpointAngleDegrees = 42.0;
    double maxWidthRatio = 2.5;
    double minAttributeRatio = 0.45;
    double localContrastRatio = 0.15;
    double minSupportRatio = 0.65;
    double minMeanSupport = 0.28;
    int maxUnsupportedRun = 5;
    double maxPathLengthRatio = 1.45;
    double maxTurnAngleDegrees = 50.0;
    int endpointExclusionRadius = 2;
    int thirdPartySkeletonClearanceRadius = 2;
    double minCompetitionMargin = 0.18;
    double attributeWeight = 3.5;
    double directionWeight = 0.8;
    double curvatureWeight = 0.55;
    bool excludeExistingSkeletonFromSupport = false;
    bool usePhysicalMetric = false;
    bool useGlobalMatching = false;
    bool useBoundaryAdjustedCandidateDistance = false;
    int supportNeighborhoodRadius = 0;
    double noConnectCost = 2.0;
    int maxExactMatchingEndpoints = 20;
    int maxEvidenceRecords = 4096;
};

struct WeakContinuityCandidate {
    int id = 0;
    int stage = 1;
    SkeletonPoint endpointA;
    SkeletonPoint endpointB;
    double endpointDistance = 0.0;
    double endpointDistanceWorld = 0.0;
    double boundaryAdjustedDistance = 0.0;
    double angleA = 180.0;
    double angleB = 180.0;
    double widthA = 0.0;
    double widthB = 0.0;
    double localBackground = 0.0;
    double minimumSupportedValue = 0.0;
    double normalizedCost = 0.0;
    double supportRatio = 0.0;
    double meanSupport = 0.0;
    int maxUnsupportedRun = 0;
    double exactSupportRatio = 0.0;
    double exactMeanSupport = 0.0;
    int exactMaxUnsupportedRun = 0;
    double neighborhoodSupportRatio = 0.0;
    double neighborhoodMeanSupport = 0.0;
    int neighborhoodMaxUnsupportedRun = 0;
    bool usedNeighborhoodSupport = false;
    double pathLength = 0.0;
    double pathLengthRatio = 0.0;
    double maxTurnAngleDegrees = 0.0;
    double competitionMarginA = 0.0;
    double competitionMarginB = 0.0;
    int finiteBackgroundSamples = 0;
    int invalidBackgroundSamples = 0;
    int evaluatedSupportPixels = 0;
    int excludedSkeletonSupportPixels = 0;
    int invalidPathSamples = 0;
    int addedPathPixels = 0;
    int addedEnvelopeCells = 0;
    int contactedObjectCount = 0;
    double minimumThirdObjectClearance = -1.0;
    int nearbyThirdObjectCount = 0;
    int spatialConflictCount = 0;
    int endpointBranchIdA = -1;
    int endpointBranchIdB = -1;
    double endpointJunctionDistanceA = -1.0;
    double endpointJunctionDistanceB = -1.0;
    int junctionClusterIdA = -1;
    int junctionClusterIdB = -1;
    int junctionArmCountA = 0;
    int junctionArmCountB = 0;
    double junctionConfidenceA = -1.0;
    double junctionConfidenceB = -1.0;
    bool junctionAttributionAvailableA = false;
    bool junctionAttributionAvailableB = false;
    bool junctionBranchMatchedA = false;
    bool junctionBranchMatchedB = false;
    bool junctionAmbiguousA = false;
    bool junctionAmbiguousB = false;
    int endpointComponentHolesA = 0;
    int endpointComponentHolesB = 0;
    int mergedComponentHolesAfter = 0;
    int localHolesBefore = 0;
    int localHolesAfter = 0;
    bool pathFound = false;
    bool accepted = false;
    bool selectedByGlobalMatching = false;
    std::string decision;
    std::vector<SkeletonPoint> path;
};

struct WeakContinuityStats {
    int endpointCount = 0;
    int distanceCandidatePairs = 0;
    int directionRejectedPairs = 0;
    int widthRejectedPairs = 0;
    int sameComponentRejectedPairs = 0;
    int preselectedPairs = 0;
    int searchedPaths = 0;
    int foundPaths = 0;
    int noPathRejected = 0;
    int invalidBackgroundRejected = 0;
    int noEvaluatedSupportRejected = 0;
    int attributeRejected = 0;
    int unsupportedRunRejected = 0;
    int curvatureRejected = 0;
    int detourRejected = 0;
    int competitionRejected = 0;
    int endpointReuseRejected = 0;
    int acceptedConnections = 0;
    int globalMatchingComponents = 0;
    int globalMatchingExactComponents = 0;
    int globalMatchingFallbackComponents = 0;
    int globalMatchingNoConnectEndpoints = 0;
    int spatialConflictPairs = 0;
    int spatialConflictRejected = 0;
    int finiteBackgroundSamples = 0;
    int invalidBackgroundSamples = 0;
    int evaluatedSupportPixels = 0;
    int excludedSkeletonSupportPixels = 0;
    int invalidPathSamples = 0;
    int addedPathPixels = 0;
};

struct WeakContinuityResult {
    BinaryMask mask;
    WeakContinuityStats stats;
    std::vector<WeakContinuityCandidate> candidates;
};

void validateWeakContinuityOptions(const WeakContinuityOptions& options);

WeakContinuityResult connectWeakContinuityPaths(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    const BinaryMask& attributeMask,
    const SkeletonGraph& graph,
    double supportThreshold,
    const WeakContinuityOptions& options,
    const BinaryMask* componentConnectivityMask = nullptr);

} // namespace fpt
