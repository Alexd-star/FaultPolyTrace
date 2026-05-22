#pragma once

#include "../core/FaultTracer.h"
#include "../core/Topology.h"

#include <string>

namespace fpt {

struct RunDiagnostics {
    std::string inputPath;
    std::string outputPath;
    std::string traceMode;
    std::string thresholdMode;
    double threshold = 0.0;
    double percentile = 0.0;
    double effectiveThreshold = 0.0;
    double minArea = 0.0;
    int minCells = 0;
    int openRadius = 0;
    int closeRadius = 0;
    double weakPercentile = 0.0;
    int weakConnectRadius = 0;
    int maxWeakComponentCells = 0;
    double minWeakComponentAspect = 0.0;
    int beltRadius = 0;
    int postBeltCloseRadius = 0;
    bool attributeConstrainedBelt = false;
    int attributeEnvelopeRadius = 0;
    int minBranchLength = 0;
    int minJunctionBranchLength = 0;
    double minJunctionBranchMean = 0.0;
    int maxGapDistance = 0;
    double maxGapAngle = 0.0;
    double minGapSupportRatio = 0.0;
    bool rootAntiMerge = false;
    int rootAntiMergeRadius = 0;
    double rootAntiMergeSupportRatio = 0.0;
    double rootAntiMergeMaxBridgeValue = 0.0;
    int rootAntiMergeMinPartCells = 0;
    double rootAntiMergeMinPartRatio = 0.0;
    double rootAntiMergeWeakGuardThreshold = 0.0;
    int rootAntiMergeWeakGuardRadius = 0;
    bool localAntiMerge = false;
    double localAntiMergeLowThreshold = 0.0;
    int localAntiMergeBridgeRadius = 0;
    int localAntiMergeMinPartCells = 0;
    double localAntiMergeMinPartRatio = 0.0;
    int localAntiMergeMinBridgeCells = 0;
    double localAntiMergeMinLowRatio = 0.0;
    bool adaptiveAreaFilter = false;
    int adaptiveAreaMinCells = 0;
    double adaptiveAreaMinAspect = 0.0;
    double adaptiveAreaMinMeanRatio = 0.0;
    double adaptiveAreaMinMaxRatio = 0.0;
    bool residualAttributeFill = false;
    bool residualTrace = false;
    double residualPercentile = 0.0;
    int residualFillRadius = 0;
    int residualMinCells = 0;
    int residualTraceMinLength = 0;
    int residualTraceMaxCells = 0;
    int residualCoveredGuardRadius = 0;
    bool residualTraceDetached = false;
    int residualDetachedMinCells = 0;
    int residualDetachedMinLength = 0;
    double residualDetachedMinAspect = 0.0;
    double residualDetachedMinMeanRatio = 0.0;
    double residualDetachedMinMaxRatio = 0.0;
    double simplifyTolerance = 0.0;
    int smoothIterations = 0;
    bool preserveCoverageAfterSmoothing = false;
    double coveragePreserveMargin = 0.0;
    int gridNx = 0;
    int gridNy = 0;
    int activeCellCount = 0;
    int polygonCount = 0;
    int keptPolygonCount = 0;
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
    int coveragePreserveSimplifyFallbackPolygons = 0;
    int coveragePreserveSmoothFallbackPolygons = 0;
    int coveragePreserveTopologyOverridePolygons = 0;
    int openRingCount = 0;
    int selfIntersectingPolygonCount = 0;
    int polygonIntersectionCount = 0;
    long long readMs = 0;
    long long traceMs = 0;
    long long topologyMs = 0;
    long long writeMs = 0;
    long long elapsedMs = 0;
};

void writeDiagnosticsJson(const std::string& path, const RunDiagnostics& diagnostics);
void writeTopologyDetailsCsv(const std::string& path, const TopologyDiagnostics& diagnostics);

} // namespace fpt
