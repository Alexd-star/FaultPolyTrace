#include "DiagnosticsIO.h"

#include <fstream>
#include <stdexcept>

namespace fpt {
namespace {

std::string escapeJson(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

} // namespace

void writeDiagnosticsJson(const std::string& path, const RunDiagnostics& d) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open diagnostics file: " + path);
    }

    out << "{\n";
    out << "  \"inputPath\": \"" << escapeJson(d.inputPath) << "\",\n";
    out << "  \"outputPath\": \"" << escapeJson(d.outputPath) << "\",\n";
    out << "  \"traceMode\": \"" << escapeJson(d.traceMode) << "\",\n";
    out << "  \"thresholdMode\": \"" << escapeJson(d.thresholdMode) << "\",\n";
    out << "  \"parameters\": {\n";
    out << "    \"threshold\": " << d.threshold << ",\n";
    out << "    \"percentile\": " << d.percentile << ",\n";
    out << "    \"effectiveThreshold\": " << d.effectiveThreshold << ",\n";
    out << "    \"minArea\": " << d.minArea << ",\n";
    out << "    \"minCells\": " << d.minCells << ",\n";
    out << "    \"openRadius\": " << d.openRadius << ",\n";
    out << "    \"closeRadius\": " << d.closeRadius << ",\n";
    out << "    \"weakPercentile\": " << d.weakPercentile << ",\n";
    out << "    \"weakConnectRadius\": " << d.weakConnectRadius << ",\n";
    out << "    \"maxWeakComponentCells\": " << d.maxWeakComponentCells << ",\n";
    out << "    \"minWeakComponentAspect\": " << d.minWeakComponentAspect << ",\n";
    out << "    \"beltRadius\": " << d.beltRadius << ",\n";
    out << "    \"postBeltCloseRadius\": " << d.postBeltCloseRadius << ",\n";
    out << "    \"attributeConstrainedBelt\": " << (d.attributeConstrainedBelt ? "true" : "false") << ",\n";
    out << "    \"attributeEnvelopeRadius\": " << d.attributeEnvelopeRadius << ",\n";
    out << "    \"minBranchLength\": " << d.minBranchLength << ",\n";
    out << "    \"minJunctionBranchLength\": " << d.minJunctionBranchLength << ",\n";
    out << "    \"minJunctionBranchMean\": " << d.minJunctionBranchMean << ",\n";
    out << "    \"maxGapDistance\": " << d.maxGapDistance << ",\n";
    out << "    \"maxGapAngle\": " << d.maxGapAngle << ",\n";
    out << "    \"minGapSupportRatio\": " << d.minGapSupportRatio << ",\n";
    out << "    \"rootAntiMerge\": " << (d.rootAntiMerge ? "true" : "false") << ",\n";
    out << "    \"rootAntiMergeRadius\": " << d.rootAntiMergeRadius << ",\n";
    out << "    \"rootAntiMergeSupportRatio\": " << d.rootAntiMergeSupportRatio << ",\n";
    out << "    \"rootAntiMergeMaxBridgeValue\": " << d.rootAntiMergeMaxBridgeValue << ",\n";
    out << "    \"rootAntiMergeMinPartCells\": " << d.rootAntiMergeMinPartCells << ",\n";
    out << "    \"rootAntiMergeMinPartRatio\": " << d.rootAntiMergeMinPartRatio << ",\n";
    out << "    \"rootAntiMergeWeakGuardThreshold\": " << d.rootAntiMergeWeakGuardThreshold << ",\n";
    out << "    \"rootAntiMergeWeakGuardRadius\": " << d.rootAntiMergeWeakGuardRadius << ",\n";
    out << "    \"localAntiMerge\": " << (d.localAntiMerge ? "true" : "false") << ",\n";
    out << "    \"localAntiMergeLowThreshold\": " << d.localAntiMergeLowThreshold << ",\n";
    out << "    \"localAntiMergeBridgeRadius\": " << d.localAntiMergeBridgeRadius << ",\n";
    out << "    \"localAntiMergeMinPartCells\": " << d.localAntiMergeMinPartCells << ",\n";
    out << "    \"localAntiMergeMinPartRatio\": " << d.localAntiMergeMinPartRatio << ",\n";
    out << "    \"localAntiMergeMinBridgeCells\": " << d.localAntiMergeMinBridgeCells << ",\n";
    out << "    \"localAntiMergeMinLowRatio\": " << d.localAntiMergeMinLowRatio << ",\n";
    out << "    \"adaptiveAreaFilter\": " << (d.adaptiveAreaFilter ? "true" : "false") << ",\n";
    out << "    \"adaptiveAreaMinCells\": " << d.adaptiveAreaMinCells << ",\n";
    out << "    \"adaptiveAreaMinAspect\": " << d.adaptiveAreaMinAspect << ",\n";
    out << "    \"adaptiveAreaMinMeanRatio\": " << d.adaptiveAreaMinMeanRatio << ",\n";
    out << "    \"adaptiveAreaMinMaxRatio\": " << d.adaptiveAreaMinMaxRatio << ",\n";
    out << "    \"residualAttributeFill\": " << (d.residualAttributeFill ? "true" : "false") << ",\n";
    out << "    \"residualTrace\": " << (d.residualTrace ? "true" : "false") << ",\n";
    out << "    \"residualPercentile\": " << d.residualPercentile << ",\n";
    out << "    \"residualFillRadius\": " << d.residualFillRadius << ",\n";
    out << "    \"residualMinCells\": " << d.residualMinCells << ",\n";
    out << "    \"residualTraceMinLength\": " << d.residualTraceMinLength << ",\n";
    out << "    \"residualTraceMaxCells\": " << d.residualTraceMaxCells << ",\n";
    out << "    \"residualCoveredGuardRadius\": " << d.residualCoveredGuardRadius << ",\n";
    out << "    \"residualTraceDetached\": " << (d.residualTraceDetached ? "true" : "false") << ",\n";
    out << "    \"residualDetachedMinCells\": " << d.residualDetachedMinCells << ",\n";
    out << "    \"residualDetachedMinLength\": " << d.residualDetachedMinLength << ",\n";
    out << "    \"residualDetachedMinAspect\": " << d.residualDetachedMinAspect << ",\n";
    out << "    \"residualDetachedMinMeanRatio\": " << d.residualDetachedMinMeanRatio << ",\n";
    out << "    \"residualDetachedMinMaxRatio\": " << d.residualDetachedMinMaxRatio << ",\n";
    out << "    \"simplifyTolerance\": " << d.simplifyTolerance << ",\n";
    out << "    \"smoothIterations\": " << d.smoothIterations << ",\n";
    out << "    \"preserveCoverageAfterSmoothing\": " << (d.preserveCoverageAfterSmoothing ? "true" : "false") << ",\n";
    out << "    \"coveragePreserveMargin\": " << d.coveragePreserveMargin << "\n";
    out << "  },\n";
    out << "  \"grid\": {\n";
    out << "    \"nx\": " << d.gridNx << ",\n";
    out << "    \"ny\": " << d.gridNy << "\n";
    out << "  },\n";
    out << "  \"results\": {\n";
    out << "    \"activeCellCount\": " << d.activeCellCount << ",\n";
    out << "    \"polygonCount\": " << d.polygonCount << ",\n";
    out << "    \"keptPolygonCount\": " << d.keptPolygonCount << ",\n";
    out << "    \"suppressedInternalRings\": " << d.suppressedInternalRings << ",\n";
    out << "    \"elapsedMs\": " << d.elapsedMs << "\n";
    out << "  },\n";
    out << "  \"skeleton\": {\n";
    out << "    \"endpoints\": " << d.skeletonEndpoints << ",\n";
    out << "    \"junctions\": " << d.skeletonJunctions << ",\n";
    out << "    \"branches\": " << d.skeletonBranches << ",\n";
    out << "    \"keptBranches\": " << d.keptBranches << ",\n";
    out << "    \"prunedBranches\": " << d.prunedBranches << ",\n";
    out << "    \"connectedGaps\": " << d.connectedGaps << ",\n";
    out << "    \"rootAntiMergeCandidateCutCells\": " << d.rootAntiMergeCandidateCutCells << ",\n";
    out << "    \"rootAntiMergeCutCells\": " << d.rootAntiMergeCutCells << ",\n";
    out << "    \"rootAntiMergeRejectedCutCells\": " << d.rootAntiMergeRejectedCutCells << ",\n";
    out << "    \"rootAntiMergeAcceptedComponents\": " << d.rootAntiMergeAcceptedComponents << ",\n";
    out << "    \"rootAntiMergeRejectedComponents\": " << d.rootAntiMergeRejectedComponents << ",\n";
    out << "    \"rootAntiMergeWeakGuardRejectedCutCells\": " << d.rootAntiMergeWeakGuardRejectedCutCells << ",\n";
    out << "    \"rootAntiMergeWeakGuardRejectedComponents\": " << d.rootAntiMergeWeakGuardRejectedComponents << ",\n";
    out << "    \"localAntiMergeCandidateCutCells\": " << d.localAntiMergeCandidateCutCells << ",\n";
    out << "    \"localAntiMergeCutCells\": " << d.localAntiMergeCutCells << ",\n";
    out << "    \"localAntiMergeRejectedCutCells\": " << d.localAntiMergeRejectedCutCells << ",\n";
    out << "    \"localAntiMergeAcceptedComponents\": " << d.localAntiMergeAcceptedComponents << ",\n";
    out << "    \"localAntiMergeRejectedComponents\": " << d.localAntiMergeRejectedComponents << ",\n";
    out << "    \"coveragePreserveSimplifyFallbackPolygons\": " << d.coveragePreserveSimplifyFallbackPolygons << ",\n";
    out << "    \"coveragePreserveSmoothFallbackPolygons\": " << d.coveragePreserveSmoothFallbackPolygons << ",\n";
    out << "    \"coveragePreserveTopologyOverridePolygons\": " << d.coveragePreserveTopologyOverridePolygons << "\n";
    out << "  },\n";
    out << "  \"topology\": {\n";
    out << "    \"openRings\": " << d.openRingCount << ",\n";
    out << "    \"selfIntersectingPolygons\": " << d.selfIntersectingPolygonCount << ",\n";
    out << "    \"polygonIntersections\": " << d.polygonIntersectionCount << "\n";
    out << "  },\n";
    out << "  \"timing\": {\n";
    out << "    \"readMs\": " << d.readMs << ",\n";
    out << "    \"traceMs\": " << d.traceMs << ",\n";
    out << "    \"topologyMs\": " << d.topologyMs << ",\n";
    out << "    \"writeMs\": " << d.writeMs << ",\n";
    out << "    \"elapsedMs\": " << d.elapsedMs << "\n";
    out << "  }\n";
    out << "}\n";
}

void writeTopologyDetailsCsv(const std::string& path, const TopologyDiagnostics& diagnostics) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open topology details file: " + path);
    }

    out << "type,polygon_a,polygon_b,edge_a,edge_b,x,y\n";
    for (const SegmentIntersectionDetail& detail : diagnostics.selfIntersectionDetails) {
        out << detail.type << ","
            << detail.polygonAId << ","
            << detail.polygonBId << ","
            << detail.edgeA << ","
            << detail.edgeB << ","
            << detail.point.x << ","
            << detail.point.y << "\n";
    }
    for (const SegmentIntersectionDetail& detail : diagnostics.polygonIntersectionDetails) {
        out << detail.type << ","
            << detail.polygonAId << ","
            << detail.polygonBId << ","
            << detail.edgeA << ","
            << detail.edgeB << ","
            << detail.point.x << ","
            << detail.point.y << "\n";
    }
}

} // namespace fpt
