#include "DiagnosticsIO.h"
#include "AtomicOutputFile.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fpt {
namespace {

std::filesystem::path filesystemPath(const std::string& utf8Path) {
#ifdef _WIN32
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           utf8Path.data(), static_cast<int>(utf8Path.size()),
                                           nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("Invalid UTF-8 file path: " + utf8Path);
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        utf8Path.data(), static_cast<int>(utf8Path.size()),
                        wide.data(), length);
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(utf8Path);
#endif
}

bool isValidUtf8(const std::string& value) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(value.data());
    std::size_t index = 0;
    while (index < value.size()) {
        const unsigned char lead = bytes[index];
        if (lead <= 0x7F) {
            ++index;
            continue;
        }

        std::size_t continuationCount = 0;
        unsigned int codePoint = 0;
        if (lead >= 0xC2 && lead <= 0xDF) {
            continuationCount = 1;
            codePoint = lead & 0x1F;
        } else if (lead >= 0xE0 && lead <= 0xEF) {
            continuationCount = 2;
            codePoint = lead & 0x0F;
        } else if (lead >= 0xF0 && lead <= 0xF4) {
            continuationCount = 3;
            codePoint = lead & 0x07;
        } else {
            return false;
        }
        if (index + continuationCount >= value.size()) {
            return false;
        }
        for (std::size_t offset = 1; offset <= continuationCount; ++offset) {
            const unsigned char continuation = bytes[index + offset];
            if ((continuation & 0xC0) != 0x80) {
                return false;
            }
            codePoint = (codePoint << 6) | (continuation & 0x3F);
        }
        if ((continuationCount == 2 && codePoint < 0x800) ||
            (continuationCount == 3 && codePoint < 0x10000) ||
            (codePoint >= 0xD800 && codePoint <= 0xDFFF) ||
            codePoint > 0x10FFFF) {
            return false;
        }
        index += continuationCount + 1;
    }
    return true;
}

std::string jsonUtf8(const std::string& value) {
#ifdef _WIN32
    if (value.empty() || isValidUtf8(value)) {
        return value;
    }
    const int wideLength = MultiByteToWideChar(
        CP_ACP, MB_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0);
    if (wideLength <= 0) {
        return value;
    }
    std::wstring wide(static_cast<std::size_t>(wideLength), L'\0');
    if (MultiByteToWideChar(
            CP_ACP, MB_ERR_INVALID_CHARS, value.data(),
            static_cast<int>(value.size()), wide.data(), wideLength) <= 0) {
        return value;
    }
    const int utf8Length = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), wideLength, nullptr, 0, nullptr, nullptr);
    if (utf8Length <= 0) {
        return value;
    }
    std::string utf8(static_cast<std::size_t>(utf8Length), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), wideLength,
        utf8.data(), utf8Length, nullptr, nullptr);
    return utf8;
#else
    return value;
#endif
}

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

std::string formatPairIds(
    const std::vector<BranchAttributionPair>& pairs) {
    std::ostringstream out;
    for (std::size_t index = 0; index < pairs.size(); ++index) {
        if (index > 0) {
            out << ";";
        }
        out << pairs[index].firstBranchId
            << "-"
            << pairs[index].secondBranchId;
    }
    return out.str();
}

std::string formatPairMetric(
    const std::vector<BranchAttributionPair>& pairs,
    double BranchAttributionPair::*member) {
    std::ostringstream out;
    out << std::setprecision(12);
    for (std::size_t index = 0; index < pairs.size(); ++index) {
        if (index > 0) {
            out << ";";
        }
        out << pairs[index].firstBranchId
            << "-"
            << pairs[index].secondBranchId
            << ":"
            << pairs[index].*member;
    }
    return out.str();
}

std::string formatIds(const std::vector<int>& ids) {
    std::ostringstream out;
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index > 0) {
            out << ";";
        }
        out << ids[index];
    }
    return out.str();
}

const char* localCandidateTypeName(LocalQualityCandidateType type) {
    switch (type) {
    case LocalQualityCandidateType::LocalBulge:
        return "local_bulge";
    case LocalQualityCandidateType::LowSupportBridge:
        return "low_support_bridge";
    case LocalQualityCandidateType::LegalWideBelt:
        return "legal_wide_belt";
    case LocalQualityCandidateType::NaturalEndCap:
        return "natural_end_cap";
    case LocalQualityCandidateType::ObjectBudgetExceeded:
        return "object_budget_exceeded";
    case LocalQualityCandidateType::InvalidGeometry:
        return "invalid_geometry";
    }
    return "unknown";
}

const char* localDecisionName(LocalQualityDecision decision) {
    switch (decision) {
    case LocalQualityDecision::Accepted:
        return "accepted";
    case LocalQualityDecision::Review:
        return "review";
    case LocalQualityDecision::Rejected:
        return "rejected";
    }
    return "unknown";
}

const char* localRejectionReasonName(LocalQualityRejectionReason reason) {
    switch (reason) {
    case LocalQualityRejectionReason::None:
        return "none";
    case LocalQualityRejectionReason::InsufficientEvidence:
        return "insufficient_evidence";
    case LocalQualityRejectionReason::NoMeaningfulChange:
        return "no_meaningful_change";
    case LocalQualityRejectionReason::ExcessiveAreaLoss:
        return "excessive_area_loss";
    case LocalQualityRejectionReason::StrongCoverageLoss:
        return "strong_coverage_loss";
    case LocalQualityRejectionReason::WeakCoverageLoss:
        return "weak_coverage_loss";
    case LocalQualityRejectionReason::BackgroundLeakage:
        return "background_leakage";
    case LocalQualityRejectionReason::TopologyRisk:
        return "topology_risk";
    case LocalQualityRejectionReason::StabilityRisk:
        return "stability_risk";
    case LocalQualityRejectionReason::InvalidGeometry:
        return "invalid_geometry";
    }
    return "unknown";
}

} // namespace

void writeDiagnosticsJson(const std::string& path, const RunDiagnostics& d) {
    AtomicOutputFile output(filesystemPath(path));
    std::ostream& out = output.stream();

    out << "{\n";
    out << "  \"inputPath\": \"" << escapeJson(jsonUtf8(d.inputPath)) << "\",\n";
    out << "  \"outputPath\": \"" << escapeJson(jsonUtf8(d.outputPath)) << "\",\n";
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
    out << "    \"weakPathGate\": " << (d.weakPathGate ? "true" : "false") << ",\n";
    out << "    \"weakPathSupportRatio\": " << d.weakPathSupportRatio << ",\n";
    out << "    \"weakPathMaxUnsupportedRun\": " << d.weakPathMaxUnsupportedRun << ",\n";
    out << "    \"branchAttributionShadow\": " << (d.branchAttributionShadow ? "true" : "false") << ",\n";
    out << "    \"branchAttributionCandidateGroups\": " << (d.branchAttributionCandidateGroups ? "true" : "false") << ",\n";
    out << "    \"localPolygonQuality\": " << (d.localPolygonQuality ? "true" : "false") << ",\n";
    out << "    \"localQualityWidthRatio\": " << d.localQualityWidthRatio << ",\n";
    out << "    \"localQualityMinExtraCells\": " << d.localQualityMinExtraCells << ",\n";
    out << "    \"localQualitySupportThresholdRatio\": " << d.localQualitySupportThresholdRatio << ",\n";
    out << "    \"localQualitySupportBufferCells\": " << d.localQualitySupportBufferCells << ",\n";
    out << "    \"localQualityMaxAreaLossRatio\": " << d.localQualityMaxAreaLossRatio << ",\n";
    out << "    \"localQualityMaxStrongCoverageLossRatio\": " << d.localQualityMaxStrongCoverageLossRatio << ",\n";
    out << "    \"localQualityMaxWeakCoverageLossRatio\": " << d.localQualityMaxWeakCoverageLossRatio << ",\n";
    out << "    \"localQualityMaxBackgroundLeakageIncreaseRatio\": " << d.localQualityMaxBackgroundLeakageIncreaseRatio << ",\n";
    out << "    \"localQualityStabilityThresholdDeltaRatio\": " << d.localQualityStabilityThresholdDeltaRatio << ",\n";
    out << "    \"localQualityMinStabilityJaccard\": " << d.localQualityMinStabilityJaccard << ",\n";
    out << "    \"localQualityMaxCandidatesPerObject\": " << d.localQualityMaxCandidatesPerObject << ",\n";
    out << "    \"localQualityMaxTotalCandidates\": " << d.localQualityMaxTotalCandidates << ",\n";
    out << "    \"localQualityMaxWindowCells\": " << d.localQualityMaxWindowCells << ",\n";
    out << "    \"localQualityMaxObjectCells\": " << d.localQualityMaxObjectCells << ",\n";
    out << "    \"localQualityMaxEvidenceRecords\": " << d.localQualityMaxEvidenceRecords << ",\n";
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
    out << "    \"residualEdgeFillRadius\": " << d.residualEdgeFillRadius << ",\n";
    out << "    \"residualEdgeFillMinValueRatio\": " << d.residualEdgeFillMinValueRatio << ",\n";
    out << "    \"residualEdgeFillMaxPatchSpan\": " << d.residualEdgeFillMaxPatchSpan << ",\n";
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
    out << "    \"residualFillCandidateCells\": " << d.residualFillCandidateCells << ",\n";
    out << "    \"residualFillAddedCells\": " << d.residualFillAddedCells << ",\n";
    out << "    \"residualFillRejectedComponents\": " << d.residualFillRejectedComponents << ",\n";
    out << "    \"coveragePreserveSimplifyFallbackPolygons\": " << d.coveragePreserveSimplifyFallbackPolygons << ",\n";
    out << "    \"coveragePreserveSmoothFallbackPolygons\": " << d.coveragePreserveSmoothFallbackPolygons << ",\n";
    out << "    \"coveragePreserveTopologyOverridePolygons\": " << d.coveragePreserveTopologyOverridePolygons << "\n";
    out << "  },\n";
    out << "  \"weakPath\": {\n";
    out << "    \"weakPathCandidateCells\": " << d.weakPathCandidateCells << ",\n";
    out << "    \"weakPathAcceptedCells\": " << d.weakPathAcceptedCells << ",\n";
    out << "    \"weakPathRejectedCells\": " << d.weakPathRejectedCells << ",\n";
    out << "    \"weakPathSeedCells\": " << d.weakPathSeedCells << ",\n";
    out << "    \"weakPathSeedContactCells\": " << d.weakPathSeedContactCells << ",\n";
    out << "    \"weakPathRootConflictRejectedCells\": " << d.weakPathRootConflictRejectedCells << ",\n";
    out << "    \"weakPathCandidateComponents\": " << d.weakPathCandidateComponents << ",\n";
    out << "    \"weakPathAcceptedComponents\": " << d.weakPathAcceptedComponents << ",\n";
    out << "    \"weakPathRejectedComponents\": " << d.weakPathRejectedComponents << ",\n";
    out << "    \"weakPathPartiallyRejectedComponents\": " << d.weakPathPartiallyRejectedComponents << ",\n";
    out << "    \"weakPathEvidenceEvaluatedComponents\": " << d.weakPathEvidenceEvaluatedFragments << ",\n";
    out << "    \"weakPathAttributeRejectedComponents\": " << d.weakPathAttributeRejectedFragments << ",\n";
    out << "    \"weakPathShapeRejectedComponents\": " << d.weakPathShapeRejectedFragments << ",\n";
    out << "    \"weakPathDirectionRejectedComponents\": " << d.weakPathDirectionRejectedFragments << ",\n";
    out << "    \"weakPathEvidenceEvaluatedFragments\": " << d.weakPathEvidenceEvaluatedFragments << ",\n";
    out << "    \"weakPathAttributeRejectedFragments\": " << d.weakPathAttributeRejectedFragments << ",\n";
    out << "    \"weakPathShapeRejectedFragments\": " << d.weakPathShapeRejectedFragments << ",\n";
    out << "    \"weakPathDirectionRejectedFragments\": " << d.weakPathDirectionRejectedFragments << "\n";
    out << "  },\n";
    out << "  \"branchAttribution\": {\n";
    out << "    \"branchAttributionJunctionClusters\": " << d.branchAttributionJunctionClusters << ",\n";
    out << "    \"branchAttributionAcceptedPairs\": " << d.branchAttributionAcceptedPairs << ",\n";
    out << "    \"branchAttributionAmbiguousClusters\": " << d.branchAttributionAmbiguousClusters << ",\n";
    out << "    \"branchAttributionUnmatchedEnds\": " << d.branchAttributionUnmatchedEnds << ",\n";
    out << "    \"branchAttributionGreedyFallbackClusters\": " << d.branchAttributionGreedyFallbackClusters << ",\n";
    out << "    \"branchAttributionPairingDifferingClusters\": " << d.branchAttributionCandidatePairingDifferingClusters << ",\n";
    out << "    \"branchAttributionDifferingClusters\": " << d.branchAttributionCandidateDifferingClusters << ",\n";
    out << "    \"branchAttributionCandidatePairingDifferingClusters\": " << d.branchAttributionCandidatePairingDifferingClusters << ",\n";
    out << "    \"branchAttributionCandidateDifferingClusters\": " << d.branchAttributionCandidateDifferingClusters << ",\n";
    out << "    \"branchAttributionAcceptedPairingDifferingClusters\": " << d.branchAttributionAcceptedPairingDifferingClusters << ",\n";
    out << "    \"branchAttributionAcceptedDifferingClusters\": " << d.branchAttributionAcceptedDifferingClusters << ",\n";
    out << "    \"branchAttributionCandidateGroupingAppliedClusters\": " << d.branchAttributionCandidateGroupingAppliedClusters << ",\n";
    out << "    \"branchAttributionCandidateGroupingAmbiguousFallbackClusters\": " << d.branchAttributionCandidateGroupingAmbiguousFallbackClusters << ",\n";
    out << "    \"branchAttributionCandidateGroupingLegacyFallbackClusters\": " << d.branchAttributionCandidateGroupingLegacyFallbackClusters << ",\n";
    out << "    \"branchAttributionCandidateGroupingComplexityFallbackClusters\": " << d.branchAttributionCandidateGroupingComplexityFallbackClusters << ",\n";
    out << "    \"branchAttributionCandidateGroupingSpanFallbackClusters\": " << d.branchAttributionCandidateGroupingSpanFallbackClusters << ",\n";
    out << "    \"branchAttributionCandidateGroupingExcludedInternalBranches\": " << d.branchAttributionCandidateGroupingExcludedInternalBranches << ",\n";
    out << "    \"branchAttributionCandidateGroupingRootGroups\": " << d.branchAttributionCandidateGroupingRootGroups << "\n";
    out << "  },\n";
    out << "  \"localPolygonQuality\": {\n";
    out << "    \"detected\": " << d.localQualityDetectedCount << ",\n";
    out << "    \"accepted\": " << d.localQualityAcceptedCount << ",\n";
    out << "    \"review\": " << d.localQualityReviewCount << ",\n";
    out << "    \"rejected\": " << d.localQualityRejectedCount << ",\n";
    out << "    \"rejectedInsufficientEvidence\": " << d.localQualityInsufficientEvidenceRejectedCount << ",\n";
    out << "    \"rejectedNoMeaningfulChange\": " << d.localQualityNoMeaningfulChangeRejectedCount << ",\n";
    out << "    \"rejectedAreaLoss\": " << d.localQualityAreaLossRejectedCount << ",\n";
    out << "    \"rejectedStrongCoverage\": " << d.localQualityStrongCoverageRejectedCount << ",\n";
    out << "    \"rejectedWeakCoverage\": " << d.localQualityWeakCoverageRejectedCount << ",\n";
    out << "    \"rejectedBackgroundLeakage\": " << d.localQualityBackgroundLeakageRejectedCount << ",\n";
    out << "    \"rejectedTopology\": " << d.localQualityTopologyRejectedCount << ",\n";
    out << "    \"rejectedStability\": " << d.localQualityStabilityRejectedCount << ",\n";
    out << "    \"rejectedInvalidGeometry\": " << d.localQualityInvalidGeometryRejectedCount << ",\n";
    out << "    \"objectBudgetReviewCount\": " << d.localQualityObjectBudgetReviewCount << ",\n";
    out << "    \"postRepairFilteredCount\": " << d.localQualityPostRepairFilteredCount << ",\n";
    out << "    \"totalEvidence\": " << d.localQualityTotalEvidence << ",\n";
    out << "    \"storedEvidence\": " << d.localQualityStoredEvidence << ",\n";
    out << "    \"truncatedEvidence\": " << d.localQualityTruncatedEvidence << ",\n";
    out << "    \"candidateEvidence\": [";
    const std::size_t evidenceCount = std::min(
        d.localPolygonQualityEvidence.size(),
        d.localQualityMaxEvidenceRecords);
    for (std::size_t index = 0;
         index < evidenceCount;
         ++index) {
        const LocalPolygonQualityEvidence& evidence =
            d.localPolygonQualityEvidence[index];
        if (index > 0) {
            out << ",";
        }
        out << "\n      {\n";
        out << "        \"objectIndex\": " << evidence.objectIndex << ",\n";
        out << "        \"type\": \"" << localCandidateTypeName(evidence.type) << "\",\n";
        out << "        \"window\": {\n";
        out << "          \"minGridX\": " << evidence.minGridX << ",\n";
        out << "          \"maxGridX\": " << evidence.maxGridX << ",\n";
        out << "          \"minGridY\": " << evidence.minGridY << ",\n";
        out << "          \"maxGridY\": " << evidence.maxGridY << "\n";
        out << "        },\n";
        out << "        \"decision\": \"" << localDecisionName(evidence.decision) << "\",\n";
        out << "        \"rejectionReason\": \""
            << localRejectionReasonName(evidence.rejectionReason)
            << "\",\n";
        out << "        \"supportRatio\": " << evidence.supportRatio << ",\n";
        out << "        \"areaLossRatio\": " << evidence.areaLossRatio << ",\n";
        out << "        \"strongCoverageLossRatio\": "
            << evidence.strongCoverageLossRatio << ",\n";
        out << "        \"weakCoverageLossRatio\": "
            << evidence.weakCoverageLossRatio << ",\n";
        out << "        \"backgroundLeakageDelta\": "
            << evidence.backgroundLeakageDelta << ",\n";
        out << "        \"stabilityJaccard\": "
            << evidence.stabilityJaccard << "\n";
        out << "      }";
    }
    if (evidenceCount > 0) {
        out << "\n    ";
    }
    out << "]\n";
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
    output.commit();
}

void writeTopologyDetailsCsv(const std::string& path, const TopologyDiagnostics& diagnostics) {
    AtomicOutputFile output(filesystemPath(path));
    std::ostream& out = output.stream();

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
    output.commit();
}

void writeBranchAttributionDifferencesCsv(
    const std::string& path,
    const std::vector<JunctionAttributionResult>& junctions) {
    AtomicOutputFile output(filesystemPath(path));
    std::ostream& out = output.stream();

    out << "cluster_id,center_grid_x,center_grid_y,center_world_x,center_world_y,"
        << "min_grid_x,min_grid_y,max_grid_x,max_grid_y,"
        << "legacy_pairs,new_pairs,legacy_unmatched,new_unmatched,"
        << "pairing_differs_from_legacy,differs_from_legacy,"
        << "ambiguous,used_exact_matching,"
        << "best_cost,alternative_cost,confidence,"
        << "turn_angle_degrees,angle_cost,attribute_cost,width_cost,background_cost,"
        << "candidate_pairing_differs_from_legacy,candidate_differs_from_legacy,"
        << "accepted_pairing_differs_from_legacy,accepted_differs_from_legacy\n";
    out << std::setprecision(12);
    for (const JunctionAttributionResult& junction : junctions) {
        out << junction.clusterId << ","
            << junction.centerGridX << ","
            << junction.centerGridY << ","
            << junction.centerWorldX << ","
            << junction.centerWorldY << ","
            << junction.minGridX << ","
            << junction.minGridY << ","
            << junction.maxGridX << ","
            << junction.maxGridY << ","
            << formatPairIds(junction.legacyPairs) << ","
            << formatPairIds(junction.pairs) << ","
            << formatIds(junction.legacyUnmatchedBranchIds) << ","
            << formatIds(junction.unmatchedBranchIds) << ","
            << (junction.pairingDiffersFromLegacy ? 1 : 0) << ","
            << (junction.differsFromLegacy ? 1 : 0) << ","
            << (junction.ambiguous ? 1 : 0) << ","
            << (junction.usedExactMatching ? 1 : 0) << ","
            << junction.bestCost << ",";
        if (std::isfinite(junction.alternativeCost)) {
            out << junction.alternativeCost;
        }
        out << ","
            << junction.confidence << ","
            << formatPairMetric(
                junction.pairs,
                &BranchAttributionPair::turnAngleDegrees)
            << ","
            << formatPairMetric(
                junction.pairs,
                &BranchAttributionPair::angleCost)
            << ","
            << formatPairMetric(
                junction.pairs,
                &BranchAttributionPair::attributeCost)
            << ","
            << formatPairMetric(
                junction.pairs,
                &BranchAttributionPair::widthCost)
            << ","
            << formatPairMetric(
                junction.pairs,
                &BranchAttributionPair::backgroundCost)
            << ","
            << (junction.pairingDiffersFromLegacy ? 1 : 0)
            << ","
            << (junction.differsFromLegacy ? 1 : 0)
            << ","
            << (junction.acceptedPairingDiffersFromLegacy ? 1 : 0)
            << ","
            << (junction.acceptedDiffersFromLegacy ? 1 : 0)
            << "\n";
    }
    output.commit();
}

} // namespace fpt
