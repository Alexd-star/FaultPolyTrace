#pragma once

#include "Polygon.h"
#include "Segmentation.h"

#include <vector>

namespace fpt {

enum class TraceMode {
    Region,
    RidgeBelt
};

struct TraceOptions {
    TraceMode traceMode = TraceMode::Region;
    ThresholdMode thresholdMode = ThresholdMode::Fixed;
    double threshold = 0.5;
    double percentile = 0.90;
    double minArea = 0.0;
    int minCells = 1;
    int openRadius = 0;
    int closeRadius = 0;
    double weakPercentile = 0.0;
    int weakConnectRadius = 0;
    int maxWeakComponentCells = 20000;
    double minWeakComponentAspect = 3.0;
    int beltRadius = 2;
    int postBeltCloseRadius = 1;
    bool attributeConstrainedBelt = false;
    int attributeEnvelopeRadius = 1;
    int minBranchLength = 4;
    int minJunctionBranchLength = 4;
    double minJunctionBranchMean = 1.0;
    int maxGapDistance = 0;
    double maxGapAngle = 35.0;
    double minGapSupportRatio = 0.25;
    bool separateBranchBelts = false;
    bool rootAntiMerge = false;
    int rootAntiMergeRadius = 1;
    double rootAntiMergeSupportRatio = 0.25;
    double rootAntiMergeMaxBridgeValue = 1e-12;
    int rootAntiMergeMinPartCells = 0;
    double rootAntiMergeMinPartRatio = 0.12;
    double rootAntiMergeWeakGuardThreshold = 0.66;
    int rootAntiMergeWeakGuardRadius = 0;
    bool localAntiMerge = false;
    double localAntiMergeLowThreshold = 0.05;
    int localAntiMergeBridgeRadius = 2;
    int localAntiMergeMinPartCells = 80;
    double localAntiMergeMinPartRatio = 0.08;
    int localAntiMergeMinBridgeCells = 6;
    double localAntiMergeMinLowRatio = 0.55;
    bool adaptiveAreaFilter = true;
    int adaptiveAreaMinCells = 4;
    double adaptiveAreaMinAspect = 2.0;
    double adaptiveAreaMinMeanRatio = 0.55;
    double adaptiveAreaMinMaxRatio = 0.90;
    bool residualAttributeFill = true;
    bool residualTrace = false;
    double residualPercentile = 0.955;
    int residualFillRadius = 2;
    int residualMinCells = 2;
    int residualTraceMinLength = 6;
    int residualTraceMaxCells = 160;
    int residualCoveredGuardRadius = 0;
    bool residualTraceDetached = true;
    int residualDetachedMinCells = 8;
    int residualDetachedMinLength = 10;
    double residualDetachedMinAspect = 3.0;
    double residualDetachedMinMeanRatio = 0.70;
    double residualDetachedMinMaxRatio = 0.92;
    double simplifyTolerance = 0.0;
    int smoothIterations = 0;
};

struct BranchTrace {
    int id = 0;
    int pointCount = 0;
    double length = 0.0;
    double meanValue = 0.0;
    double maxValue = 0.0;
    bool kept = true;
    bool startIsJunction = false;
    bool endIsJunction = false;
    std::vector<Point> points;
};

struct TraceResult {
    std::vector<Polygon> polygons;
    std::vector<BranchTrace> branches;
    int activeCellCount = 0;
    double effectiveThreshold = 0.0;
    int skeletonEndpoints = 0;
    int skeletonJunctions = 0;
    int skeletonBranches = 0;
    int keptBranches = 0;
    int prunedBranches = 0;
    int connectedGaps = 0;
    int rootAntiMergeCandidateCutCells = 0;
    int rootAntiMergeCutCells = 0;
    int rootAntiMergeRejectedCutCells = 0;
    int rootAntiMergeAcceptedComponents = 0;
    int rootAntiMergeRejectedComponents = 0;
    int rootAntiMergeWeakGuardRejectedCutCells = 0;
    int rootAntiMergeWeakGuardRejectedComponents = 0;
    int localAntiMergeCandidateCutCells = 0;
    int localAntiMergeCutCells = 0;
    int localAntiMergeRejectedCutCells = 0;
    int localAntiMergeAcceptedComponents = 0;
    int localAntiMergeRejectedComponents = 0;
    int suppressedInternalRings = 0;
};

TraceResult traceFaultPolygons(const Grid2D& grid, const TraceOptions& options);

} // namespace fpt
