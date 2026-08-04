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
    out << "  \"format\": \"FaultPolyTrace.RunDiagnostics\",\n";
    out << "  \"schemaVersion\": 2,\n";
    out << "  \"inputPath\": \"" << escapeJson(jsonUtf8(d.inputPath)) << "\",\n";
    out << "  \"outputPath\": \"" << escapeJson(jsonUtf8(d.outputPath)) << "\",\n";
    out << "  \"traceMode\": \"" << escapeJson(d.traceMode) << "\",\n";
    out << "  \"thresholdMode\": \"" << escapeJson(d.thresholdMode) << "\",\n";
    out << "  \"parameters\": {\n";
    out << "    \"threshold\": " << d.threshold << ",\n";
    out << "    \"percentile\": " << d.percentile << ",\n";
    out << "    \"effectiveThreshold\": " << d.effectiveThreshold << ",\n";
    const auto writeThresholdInfo = [&](const char* name,
                                        const PercentileThresholdInfo& info,
                                        bool trailingComma) {
        out << "    \"" << name << "\": {"
            << "\"threshold\": " << info.threshold
            << ", \"stablePercentileLowerInclusive\": "
            << info.stablePercentileLowerInclusive
            << ", \"stablePercentileUpperExclusive\": "
            << info.stablePercentileUpperExclusive
            << ", \"sampleCount\": " << info.sampleCount
            << ", \"equalValueCount\": " << info.equalValueCount
            << ", \"percentileBased\": "
            << (info.percentileBased ? "true" : "false") << "}"
            << (trailingComma ? "," : "") << "\n";
    };
    writeThresholdInfo("primaryThresholdInfo", d.primaryThresholdInfo, true);
    writeThresholdInfo("weakThresholdInfo", d.weakThresholdInfo, true);
    writeThresholdInfo("residualThresholdInfo", d.residualThresholdInfo, true);
    out << "    \"minArea\": " << d.minArea << ",\n";
    out << "    \"minCells\": " << d.minCells << ",\n";
    out << "    \"openRadius\": " << d.openRadius << ",\n";
    out << "    \"closeRadius\": " << d.closeRadius << ",\n";
    out << "    \"physicalMorphologyShadow\": "
        << (d.physicalMorphologyShadow ? "true" : "false") << ",\n";
    out << "    \"physicalCloseRadiusWorld\": "
        << d.physicalCloseRadiusWorld << ",\n";
    out << "    \"weakPercentile\": " << d.weakPercentile << ",\n";
    out << "    \"weakConnectRadius\": " << d.weakConnectRadius << ",\n";
    out << "    \"maxWeakComponentCells\": " << d.maxWeakComponentCells << ",\n";
    out << "    \"minWeakComponentAspect\": " << d.minWeakComponentAspect << ",\n";
    out << "    \"weakPathGate\": " << (d.weakPathGate ? "true" : "false") << ",\n";
    out << "    \"weakExtensionLayeredShadow\": "
        << (d.weakExtensionLayeredShadow ? "true" : "false") << ",\n";
    out << "    \"weakExtensionLayeredGate\": "
        << (d.weakExtensionLayeredGate ? "true" : "false") << ",\n";
    out << "    \"weakPathSupportRatio\": " << d.weakPathSupportRatio << ",\n";
    out << "    \"weakPathMaxUnsupportedRun\": " << d.weakPathMaxUnsupportedRun << ",\n";
    out << "    \"branchAttributionShadow\": " << (d.branchAttributionShadow ? "true" : "false") << ",\n";
    out << "    \"branchAttributionCandidateGroups\": " << (d.branchAttributionCandidateGroups ? "true" : "false") << ",\n";
    out << "    \"skeletonGraphV2Shadow\": "
        << (d.skeletonGraphV2Shadow ? "true" : "false") << ",\n";
    out << "    \"skeletonGraphV2AttributionShadow\": "
        << (d.skeletonGraphV2AttributionShadow ? "true" : "false") << ",\n";
    out << "    \"multiScaleEvidenceShadow\": "
        << (d.multiScaleEvidenceShadow ? "true" : "false") << ",\n";
    out << "    \"multiScaleEvidenceMinimumConfidence\": "
        << d.multiScaleEvidenceMinimumConfidence << ",\n";
    out << "    \"multiScaleEvidenceMinimumRidgeResponse\": "
        << d.multiScaleEvidenceMinimumRidgeResponse << ",\n";
    out << "    \"multiScaleEvidenceMaximumSamples\": "
        << d.multiScaleEvidenceMaximumSamples << ",\n";
    out << "    \"adaptiveBoundaryShadow\": "
        << (d.adaptiveBoundaryShadow ? "true" : "false") << ",\n";
    out << "    \"adaptiveBoundaryApply\": "
        << (d.adaptiveBoundaryApply ? "true" : "false") << ",\n";
    out << "    \"adaptiveBoundaryMaximumDisplacementWorld\": "
        << d.adaptiveBoundaryMaximumDisplacementWorld << ",\n";
    out << "    \"adaptiveBoundaryMinimumSupportGain\": "
        << d.adaptiveBoundaryMinimumSupportGain << ",\n";
    out << "    \"adaptiveBoundaryMaximumSupportLossRatio\": "
        << d.adaptiveBoundaryMaximumSupportLossRatio << ",\n";
    out << "    \"adaptiveBoundaryMaximumLeakageIncreaseRatio\": "
        << d.adaptiveBoundaryMaximumLeakageIncreaseRatio << ",\n";
    out << "    \"adaptiveBoundaryMaximumPolygons\": "
        << d.adaptiveBoundaryMaximumPolygons << ",\n";
    out << "    \"adaptiveBoundaryMaximumVerticesPerPolygon\": "
        << d.adaptiveBoundaryMaximumVerticesPerPolygon << ",\n";
    out << "    \"localPolygonQualityShadow\": " << (d.localPolygonQualityShadow ? "true" : "false") << ",\n";
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
    out << "    \"weakContinuityShadow\": " << (d.weakContinuityShadow ? "true" : "false") << ",\n";
    out << "    \"weakContinuity\": " << (d.weakContinuity ? "true" : "false") << ",\n";
    out << "    \"weakContinuitySecondPass\": " << (d.weakContinuitySecondPass ? "true" : "false") << ",\n";
    out << "    \"weakContinuityFinalObjectPass\": " << (d.weakContinuityFinalObjectPass ? "true" : "false") << ",\n";
    out << "    \"weakContinuityMaxDistance\": " << d.weakContinuityMaxDistance << ",\n";
    out << "    \"weakContinuityTangentWindow\": " << d.weakContinuityTangentWindow << ",\n";
    out << "    \"weakContinuityCorridorRadius\": " << d.weakContinuityCorridorRadius << ",\n";
    out << "    \"weakContinuityMaxCandidatesPerEndpoint\": " << d.weakContinuityMaxCandidatesPerEndpoint << ",\n";
    out << "    \"weakContinuityMaxEndpointAngle\": " << d.weakContinuityMaxEndpointAngle << ",\n";
    out << "    \"weakContinuityMaxWidthRatio\": " << d.weakContinuityMaxWidthRatio << ",\n";
    out << "    \"weakContinuityMinAttributeRatio\": " << d.weakContinuityMinAttributeRatio << ",\n";
    out << "    \"weakContinuityReferenceThresholdRatio\": " << d.weakContinuityReferenceThresholdRatio << ",\n";
    out << "    \"weakContinuityLocalContrastRatio\": " << d.weakContinuityLocalContrastRatio << ",\n";
    out << "    \"weakContinuityMinSupportRatio\": " << d.weakContinuityMinSupportRatio << ",\n";
    out << "    \"weakContinuityMinMeanSupport\": " << d.weakContinuityMinMeanSupport << ",\n";
    out << "    \"weakContinuityMaxUnsupportedRun\": " << d.weakContinuityMaxUnsupportedRun << ",\n";
    out << "    \"weakContinuityMaxPathLengthRatio\": " << d.weakContinuityMaxPathLengthRatio << ",\n";
    out << "    \"weakContinuityMaxTurnAngle\": " << d.weakContinuityMaxTurnAngle << ",\n";
    out << "    \"weakContinuityMinCompetitionMargin\": " << d.weakContinuityMinCompetitionMargin << ",\n";
    out << "    \"weakContinuityThirdPartySkeletonClearanceRadius\": " << d.weakContinuityThirdPartySkeletonClearanceRadius << ",\n";
    out << "    \"weakContinuityUsePhysicalMetric\": "
        << (d.weakContinuityUsePhysicalMetric ? "true" : "false") << ",\n";
    out << "    \"weakContinuityUseGlobalMatching\": "
        << (d.weakContinuityUseGlobalMatching ? "true" : "false") << ",\n";
    out << "    \"weakContinuityNoConnectCost\": "
        << d.weakContinuityNoConnectCost << ",\n";
    out << "    \"weakContinuityMaxExactMatchingEndpoints\": "
        << d.weakContinuityMaxExactMatchingEndpoints << ",\n";
    out << "    \"weakTailCompletionShadow\": " << (d.weakTailCompletionShadow ? "true" : "false") << ",\n";
    out << "    \"weakTailCompletion\": " << (d.weakTailCompletion ? "true" : "false") << ",\n";
    out << "    \"weakTailTopologyGate\": "
        << (d.weakTailTopologyGate ? "true" : "false") << ",\n";
    out << "    \"simpleRingCompatibility\": "
        << (d.simpleRingCompatibility ? "true" : "false") << ",\n";
    out << "    \"simpleRingMaximumFillCells\": "
        << d.simpleRingMaximumFillCells << ",\n";
    out << "    \"simpleRingMaximumOpeningCells\": "
        << d.simpleRingMaximumOpeningCells << ",\n";
    out << "    \"weakTailEndpointPortShadow\": "
        << (d.weakTailEndpointPortShadow ? "true" : "false") << ",\n";
    out << "    \"weakTailEndpointPortOwnership\": "
        << (d.weakTailEndpointPortOwnership ? "true" : "false") << ",\n";
    out << "    \"weakTailMinCells\": " << d.weakTailMinCells << ",\n";
    out << "    \"weakTailMaxCells\": " << d.weakTailMaxCells << ",\n";
    out << "    \"weakTailMinAspect\": " << d.weakTailMinAspect << ",\n";
    out << "    \"weakTailMinMeanSupportRatio\": " << d.weakTailMinMeanSupportRatio << ",\n";
    out << "    \"weakTailMinPeakSupportRatio\": " << d.weakTailMinPeakSupportRatio << ",\n";
    out << "    \"weakTailMaxAlignmentAngle\": " << d.weakTailMaxAlignmentAngle << ",\n";
    out << "    \"weakTailOwnershipClearanceRadius\": " << d.weakTailOwnershipClearanceRadius << ",\n";
    out << "    \"weakTailTangentRadius\": " << d.weakTailTangentRadius << ",\n";
    out << "    \"weakTailJunctionClearanceRadius\": "
        << d.weakTailJunctionClearanceRadius << ",\n";
    out << "    \"preserveObservedSkeletonEnvelope\": "
        << (d.preserveObservedSkeletonEnvelope ? "true" : "false") << ",\n";
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
    out << "  \"physicalMorphologyShadow\": {\n";
    out << "    \"evaluated\": "
        << (d.physicalMorphologyShadowEvaluated ? "true" : "false") << ",\n";
    out << "    \"radiusWorld\": " << d.physicalMorphologyRadiusWorld << ",\n";
    out << "    \"legacyCells\": " << d.physicalMorphologyLegacyCells << ",\n";
    out << "    \"candidateCells\": " << d.physicalMorphologyCandidateCells << ",\n";
    out << "    \"addedCells\": " << d.physicalMorphologyAddedCells << ",\n";
    out << "    \"removedCells\": " << d.physicalMorphologyRemovedCells << ",\n";
    out << "    \"jaccard\": " << d.physicalMorphologyJaccard << "\n";
    out << "  },\n";
    out << "  \"skeleton\": {\n";
    out << "    \"endpoints\": " << d.skeletonEndpoints << ",\n";
    out << "    \"junctions\": " << d.skeletonJunctions << ",\n";
    out << "    \"branches\": " << d.skeletonBranches << ",\n";
    out << "    \"keptBranches\": " << d.keptBranches << ",\n";
    out << "    \"prunedBranches\": " << d.prunedBranches << ",\n";
    out << "    \"connectedGaps\": " << d.connectedGaps << ",\n";
    out << "    \"graphV2Shadow\": {\n";
    out << "      \"evaluated\": "
        << (d.skeletonGraphV2ShadowEvaluated ? "true" : "false") << ",\n";
    out << "      \"skeletonPixels\": " << d.skeletonGraphV2Stats.skeletonPixels << ",\n";
    out << "      \"endpointPixels\": " << d.skeletonGraphV2Stats.endpointPixels << ",\n";
    out << "      \"rawJunctionPixels\": " << d.skeletonGraphV2Stats.rawJunctionPixels << ",\n";
    out << "      \"junctionClusters\": " << d.skeletonGraphV2Stats.junctionClusters << ",\n";
    out << "      \"isolatedPixels\": " << d.skeletonGraphV2Stats.isolatedPixels << ",\n";
    out << "      \"loopNodes\": " << d.skeletonGraphV2Stats.loopNodes << ",\n";
    out << "      \"branches\": " << d.skeletonGraphV2Stats.branches << ",\n";
    out << "      \"ports\": " << d.skeletonGraphV2Stats.ports << ",\n";
    out << "      \"halfEdges\": " << d.skeletonGraphV2Stats.halfEdges << ",\n";
    out << "      \"ambiguousPortAttachments\": "
        << d.skeletonGraphV2Stats.ambiguousPortAttachments << ",\n";
    out << "      \"syntheticEndpointNodes\": "
        << d.skeletonGraphV2Stats.syntheticEndpointNodes << "\n";
    out << "    },\n";
    out << "    \"graphV2AttributionShadow\": {\n";
    out << "      \"evaluated\": "
        << (d.skeletonGraphV2AttributionShadowEvaluated ? "true" : "false") << ",\n";
    out << "      \"legacyBranchCount\": "
        << d.skeletonGraphV2Attribution.legacyBranchCount << ",\n";
    out << "      \"v2BranchCount\": "
        << d.skeletonGraphV2Attribution.v2BranchCount << ",\n";
    out << "      \"legacyJunctionClusters\": "
        << d.skeletonGraphV2Attribution.legacyJunctionClusters << ",\n";
    out << "      \"v2JunctionClusters\": "
        << d.skeletonGraphV2Attribution.v2JunctionClusters << ",\n";
    out << "      \"matchedJunctionClusters\": "
        << d.skeletonGraphV2Attribution.matchedJunctionClusters << ",\n";
    out << "      \"equivalentMatchedJunctionClusters\": "
        << d.skeletonGraphV2Attribution.equivalentMatchedJunctionClusters << ",\n";
    out << "      \"differingMatchedJunctionClusters\": "
        << d.skeletonGraphV2Attribution.differingMatchedJunctionClusters << ",\n";
    out << "      \"legacyOnlyJunctionClusters\": "
        << d.skeletonGraphV2Attribution.legacyOnlyJunctionClusters << ",\n";
    out << "      \"v2OnlyJunctionClusters\": "
        << d.skeletonGraphV2Attribution.v2OnlyJunctionClusters << ",\n";
    out << "      \"ambiguityImprovedJunctionClusters\": "
        << d.skeletonGraphV2Attribution.ambiguityImprovedJunctionClusters << ",\n";
    out << "      \"ambiguityRegressedJunctionClusters\": "
        << d.skeletonGraphV2Attribution.ambiguityRegressedJunctionClusters << ",\n";
    out << "      \"v2AmbiguousPortAttachments\": "
        << d.skeletonGraphV2Attribution.v2AmbiguousPortAttachments << ",\n";
    out << "      \"meanMatchedCenterDistanceCells\": "
        << d.skeletonGraphV2Attribution.meanMatchedCenterDistanceCells << ",\n";
    out << "      \"maximumMatchedCenterDistanceCells\": "
        << d.skeletonGraphV2Attribution.maximumMatchedCenterDistanceCells << "\n";
    out << "    },\n";
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
    out << "  \"multiScaleEvidenceShadow\": {\n";
    out << "    \"evaluated\": "
        << (d.multiScaleEvidenceShadowEvaluated ? "true" : "false") << ",\n";
    out << "    \"targetCells\": " << d.multiScaleEvidenceSummary.targetCells << ",\n";
    out << "    \"evaluatedCells\": " << d.multiScaleEvidenceSummary.evaluatedCells << ",\n";
    out << "    \"finiteCells\": " << d.multiScaleEvidenceSummary.finiteCells << ",\n";
    out << "    \"confidentCells\": " << d.multiScaleEvidenceSummary.confidentCells << ",\n";
    out << "    \"truncatedCells\": " << d.multiScaleEvidenceSummary.truncatedCells << ",\n";
    out << "    \"invalidNeighborhoods\": " << d.multiScaleEvidenceSummary.invalidNeighborhoods << ",\n";
    out << "    \"meanRidgeResponse\": " << d.multiScaleEvidenceSummary.meanRidgeResponse << ",\n";
    out << "    \"meanConfidence\": " << d.multiScaleEvidenceSummary.meanConfidence << ",\n";
    out << "    \"maximumRidgeResponse\": " << d.multiScaleEvidenceSummary.maximumRidgeResponse << ",\n";
    out << "    \"skeletonCellsEvaluated\": " << d.multiScaleEvidenceSummary.skeletonCellsEvaluated << ",\n";
    out << "    \"skeletonOrientationAgreementCells\": "
        << d.multiScaleEvidenceSummary.skeletonOrientationAgreementCells << ",\n";
    out << "    \"meanSkeletonOrientationAngle\": "
        << d.multiScaleEvidenceSummary.meanSkeletonOrientationAngle << "\n";
    out << "  },\n";
    out << "  \"adaptiveBoundaryShadow\": {\n";
    out << "    \"evaluated\": "
        << (d.adaptiveBoundaryShadowEvaluated ? "true" : "false") << ",\n";
    out << "    \"applied\": "
        << (d.adaptiveBoundaryApplied ? "true" : "false") << ",\n";
    out << "    \"commitAttemptedCandidates\": "
        << d.adaptiveBoundaryCommitSummary.attemptedCandidates << ",\n";
    out << "    \"commitCommittedCandidates\": "
        << d.adaptiveBoundaryCommitSummary.committedCandidates << ",\n";
    out << "    \"commitMissingPolygonCandidates\": "
        << d.adaptiveBoundaryCommitSummary.missingPolygonCandidates << ",\n";
    out << "    \"commitGlobalConflictRejectedCandidates\": "
        << d.adaptiveBoundaryCommitSummary.globalConflictRejectedCandidates << ",\n";
    out << "    \"commitBatchRolledBack\": "
        << (d.adaptiveBoundaryCommitSummary.batchRolledBack ? "true" : "false")
        << ",\n";
    out << "    \"polygonsEvaluated\": " << d.adaptiveBoundarySummary.polygonsEvaluated << ",\n";
    out << "    \"noChange\": " << d.adaptiveBoundarySummary.noChange << ",\n";
    out << "    \"acceptedCandidates\": " << d.adaptiveBoundarySummary.acceptedCandidates << ",\n";
    out << "    \"reviewCandidates\": " << d.adaptiveBoundarySummary.reviewCandidates << ",\n";
    out << "    \"rejectedCandidates\": " << d.adaptiveBoundarySummary.rejectedCandidates << ",\n";
    out << "    \"topologyRejectedCandidates\": "
        << d.adaptiveBoundarySummary.topologyRejectedCandidates << ",\n";
    out << "    \"rasterBudgetReviewCandidates\": "
        << d.adaptiveBoundarySummary.rasterBudgetReviewCandidates << ",\n";
    out << "    \"movedVertices\": " << d.adaptiveBoundarySummary.movedVertices << ",\n";
    out << "    \"truncatedPolygons\": " << d.adaptiveBoundarySummary.truncatedPolygons << ",\n";
    out << "    \"rasterCellsEvaluated\": " << d.adaptiveBoundarySummary.rasterCellsEvaluated << ",\n";
    out << "    \"maximumDisplacementWorld\": "
        << d.adaptiveBoundarySummary.maximumDisplacementWorld << ",\n";
    out << "    \"meanSupportGainRatio\": "
        << d.adaptiveBoundarySummary.meanSupportGainRatio << ",\n";
    out << "    \"meanWeakSupportGainRatio\": "
        << d.adaptiveBoundarySummary.meanWeakSupportGainRatio << ",\n";
    out << "    \"meanBoundaryScoreGain\": "
        << d.adaptiveBoundarySummary.meanBoundaryScoreGain << ",\n";
    out << "    \"meanBackgroundLeakageIncreaseRatio\": "
        << d.adaptiveBoundarySummary.meanBackgroundLeakageIncreaseRatio << ",\n";
    out << "    \"evidence\": [\n";
    for (std::size_t index = 0; index < d.adaptiveBoundaryEvidence.size(); ++index) {
        const AdaptiveBoundaryEvidence& evidence = d.adaptiveBoundaryEvidence[index];
        out << "      {\"polygonId\": " << evidence.polygonId
            << ", \"decision\": \"" << adaptiveBoundaryDecisionName(evidence.decision)
            << "\", \"reason\": \"" << escapeJson(evidence.reason)
            << "\", \"movedVertices\": " << evidence.movedVertices
            << ", \"maximumDisplacementWorld\": " << evidence.maximumDisplacementWorld
            << ", \"supportGainRatio\": " << evidence.supportGainRatio
            << ", \"boundaryScoreGain\": " << evidence.boundaryScoreGain
            << ", \"supportLossRatio\": " << evidence.supportLossRatio
            << ", \"weakSupportGainRatio\": "
            << evidence.weakSupportGainRatio
            << ", \"weakSupportLossRatio\": "
            << evidence.weakSupportLossRatio
            << ", \"backgroundLeakageIncreaseRatio\": "
            << evidence.backgroundLeakageIncreaseRatio
            << ", \"areaChangeRatio\": " << evidence.areaChangeRatio
            << ", \"originalRoughness\": " << evidence.originalRoughness
            << ", \"candidateRoughness\": " << evidence.candidateRoughness
            << ", \"roughnessChangeRatio\": " << evidence.roughnessChangeRatio
            << ", \"topologyValid\": " << (evidence.topologyValid ? "true" : "false")
            << ", \"rasterBudgetExceeded\": "
            << (evidence.rasterBudgetExceeded ? "true" : "false") << "}";
        if (index + 1 < d.adaptiveBoundaryEvidence.size()) {
            out << ",";
        }
        out << "\n";
    }
    out << "    ]\n";
    out << "  },\n";
    out << "  \"residualTrace\": {\n";
    out << "    \"effectiveThreshold\": " << d.residualTraceEffectiveThreshold << ",\n";
    out << "    \"candidateComponents\": " << d.residualTraceCandidateComponents << ",\n";
    out << "    \"sizeRejectedComponents\": " << d.residualTraceSizeRejectedComponents << ",\n";
    out << "    \"shapeRejectedComponents\": " << d.residualTraceShapeRejectedComponents << ",\n";
    out << "    \"attributeRejectedComponents\": " << d.residualTraceAttributeRejectedComponents << ",\n";
    out << "    \"detachedRejectedComponents\": " << d.residualTraceDetachedRejectedComponents << ",\n";
    out << "    \"geometryRejectedComponents\": " << d.residualTraceGeometryRejectedComponents << ",\n";
    out << "    \"topologyRejectedComponents\": " << d.residualTraceTopologyRejectedComponents << ",\n";
    out << "    \"fallbackAcceptedComponents\": " << d.residualTraceFallbackAcceptedComponents << ",\n";
    out << "    \"acceptedComponents\": " << d.residualTraceAcceptedComponents << ",\n";
    out << "    \"acceptedPolygons\": " << d.residualTraceAcceptedPolygons << ",\n";
    out << "    \"acceptedCells\": " << d.residualTraceAcceptedCells << "\n";
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
    out << "  \"weakExtensionLayered\": {\n";
    out << "    \"evaluated\": "
        << (d.weakExtensionLayeredShadowEvaluated ? "true" : "false") << ",\n";
    out << "    \"componentCount\": " << d.weakExtensionComponentCount << ",\n";
    out << "    \"legacyAcceptedComponents\": "
        << d.weakExtensionLegacyAcceptedComponents << ",\n";
    out << "    \"layeredAcceptedComponents\": "
        << d.weakExtensionLayeredAcceptedComponents << ",\n";
    out << "    \"layeredRejectedComponents\": "
        << d.weakExtensionLayeredRejectedComponents << ",\n";
    out << "    \"ownerRejectedComponents\": "
        << d.weakExtensionLayeredOwnerRejectedComponents << ",\n";
    out << "    \"sizeRejectedComponents\": "
        << d.weakExtensionLayeredSizeRejectedComponents << ",\n";
    out << "    \"shapeRejectedComponents\": "
        << d.weakExtensionLayeredShapeRejectedComponents << ",\n";
    out << "    \"supportRejectedComponents\": "
        << d.weakExtensionLayeredSupportRejectedComponents << ",\n";
    out << "    \"widthRejectedComponents\": "
        << d.weakExtensionLayeredWidthRejectedComponents << ",\n";
    out << "    \"legacyOnlyCells\": " << d.weakExtensionLegacyOnlyCells << ",\n";
    out << "    \"layeredOnlyCells\": " << d.weakExtensionLayeredOnlyCells << "\n";
    out << "  },\n";
    out << "  \"weakContinuity\": {\n";
    out << "    \"endpointCount\": " << d.weakContinuityStats.endpointCount << ",\n";
    out << "    \"distanceCandidatePairs\": " << d.weakContinuityStats.distanceCandidatePairs << ",\n";
    out << "    \"directionRejectedPairs\": " << d.weakContinuityStats.directionRejectedPairs << ",\n";
    out << "    \"widthRejectedPairs\": " << d.weakContinuityStats.widthRejectedPairs << ",\n";
    out << "    \"sameComponentRejectedPairs\": " << d.weakContinuityStats.sameComponentRejectedPairs << ",\n";
    out << "    \"preselectedPairs\": " << d.weakContinuityStats.preselectedPairs << ",\n";
    out << "    \"searchedPaths\": " << d.weakContinuityStats.searchedPaths << ",\n";
    out << "    \"foundPaths\": " << d.weakContinuityStats.foundPaths << ",\n";
    out << "    \"noPathRejected\": " << d.weakContinuityStats.noPathRejected << ",\n";
    out << "    \"invalidBackgroundRejected\": " << d.weakContinuityStats.invalidBackgroundRejected << ",\n";
    out << "    \"noEvaluatedSupportRejected\": " << d.weakContinuityStats.noEvaluatedSupportRejected << ",\n";
    out << "    \"attributeRejected\": " << d.weakContinuityStats.attributeRejected << ",\n";
    out << "    \"unsupportedRunRejected\": " << d.weakContinuityStats.unsupportedRunRejected << ",\n";
    out << "    \"curvatureRejected\": " << d.weakContinuityStats.curvatureRejected << ",\n";
    out << "    \"detourRejected\": " << d.weakContinuityStats.detourRejected << ",\n";
    out << "    \"competitionRejected\": " << d.weakContinuityStats.competitionRejected << ",\n";
    out << "    \"endpointReuseRejected\": " << d.weakContinuityStats.endpointReuseRejected << ",\n";
    out << "    \"acceptedConnections\": " << d.weakContinuityStats.acceptedConnections << ",\n";
    out << "    \"globalMatchingComponents\": "
        << d.weakContinuityStats.globalMatchingComponents << ",\n";
    out << "    \"globalMatchingExactComponents\": "
        << d.weakContinuityStats.globalMatchingExactComponents << ",\n";
    out << "    \"globalMatchingFallbackComponents\": "
        << d.weakContinuityStats.globalMatchingFallbackComponents << ",\n";
    out << "    \"globalMatchingNoConnectEndpoints\": "
        << d.weakContinuityStats.globalMatchingNoConnectEndpoints << ",\n";
    out << "    \"spatialConflictPairs\": "
        << d.weakContinuityStats.spatialConflictPairs << ",\n";
    out << "    \"spatialConflictRejected\": "
        << d.weakContinuityStats.spatialConflictRejected << ",\n";
    out << "    \"finiteBackgroundSamples\": " << d.weakContinuityStats.finiteBackgroundSamples << ",\n";
    out << "    \"invalidBackgroundSamples\": " << d.weakContinuityStats.invalidBackgroundSamples << ",\n";
    out << "    \"evaluatedSupportPixels\": " << d.weakContinuityStats.evaluatedSupportPixels << ",\n";
    out << "    \"excludedSkeletonSupportPixels\": " << d.weakContinuityStats.excludedSkeletonSupportPixels << ",\n";
    out << "    \"invalidPathSamples\": " << d.weakContinuityStats.invalidPathSamples << ",\n";
    out << "    \"addedPathPixels\": " << d.weakContinuityStats.addedPathPixels << ",\n";
    out << "    \"protectedFromAntiMergeCells\": "
        << d.weakContinuityProtectedFromAntiMergeCells << ",\n";
    out << "    \"finalObjectAcceptedConnections\": "
        << d.weakContinuityFinalObjectAcceptedConnections << ",\n";
    out << "    \"finalObjectRejectedConnections\": "
        << d.weakContinuityFinalObjectRejectedConnections << ",\n";
    out << "    \"finalObjectAddedEnvelopeCells\": "
        << d.weakContinuityFinalObjectAddedEnvelopeCells << "\n";
    out << "  },\n";
    out << "  \"weakTailCompletion\": {\n";
    out << "    \"componentCount\": " << d.weakTailStats.componentCount << ",\n";
    out << "    \"sizeRejected\": " << d.weakTailStats.sizeRejected << ",\n";
    out << "    \"shapeRejected\": " << d.weakTailStats.shapeRejected << ",\n";
    out << "    \"attributeRejected\": " << d.weakTailStats.attributeRejected << ",\n";
    out << "    \"ownershipRejected\": " << d.weakTailStats.ownershipRejected << ",\n";
    out << "    \"directContactRejected\": " << d.weakTailStats.directContactRejected << ",\n";
    out << "    \"endpointPortRejected\": " << d.weakTailStats.endpointPortRejected << ",\n";
    out << "    \"junctionGuardRejected\": " << d.weakTailStats.junctionGuardRejected << ",\n";
    out << "    \"directionRejected\": " << d.weakTailStats.directionRejected << ",\n";
    out << "    \"topologyGateEvaluatedComponents\": "
        << d.weakTailStats.topologyGateEvaluatedComponents << ",\n";
    out << "    \"topologyRejectedComponents\": "
        << d.weakTailStats.topologyRejectedComponents << ",\n";
    out << "    \"topologyFallbackRejectedComponents\": "
        << d.weakTailStats.topologyFallbackRejectedComponents << ",\n";
    out << "    \"topologyRejectedCells\": "
        << d.weakTailStats.topologyRejectedCells << ",\n";
    out << "    \"topologyTrimmedComponents\": "
        << d.weakTailStats.topologyTrimmedComponents << ",\n";
    out << "    \"topologyTrimmedCells\": "
        << d.weakTailStats.topologyTrimmedCells << ",\n";
    out << "    \"topologyInitialNewEnclosures\": "
        << d.weakTailStats.topologyInitialNewEnclosures << ",\n";
    out << "    \"topologyInitialNewEnclosureCells\": "
        << d.weakTailStats.topologyInitialNewEnclosureCells << ",\n";
    out << "    \"topologyRemainingNewEnclosures\": "
        << d.weakTailStats.topologyRemainingNewEnclosures << ",\n";
    out << "    \"topologyRemainingNewEnclosureCells\": "
        << d.weakTailStats.topologyRemainingNewEnclosureCells << ",\n";
    out << "    \"acceptedComponents\": " << d.weakTailStats.acceptedComponents << ",\n";
    out << "    \"acceptedCells\": " << d.weakTailStats.acceptedCells << ",\n";
    out << "    \"protectedFromAntiMergeCells\": "
        << d.weakTailProtectedFromAntiMergeCells << "\n";
    out << "  },\n";
    out << "  \"simpleRingCompatibility\": {\n";
    out << "    \"applied\": "
        << (d.simpleRingCompatibilityApplied ? "true" : "false") << ",\n";
    out << "    \"initialHoleCount\": "
        << d.simpleRingCompatibilityStats.initialHoleCount << ",\n";
    out << "    \"initialHoleCells\": "
        << d.simpleRingCompatibilityStats.initialHoleCells << ",\n";
    out << "    \"filledHoleCount\": "
        << d.simpleRingCompatibilityStats.filledHoleCount << ",\n";
    out << "    \"filledCells\": "
        << d.simpleRingCompatibilityStats.filledCells << ",\n";
    out << "    \"fillRejectedComponents\": "
        << d.simpleRingCompatibilityStats.fillRejectedComponents << ",\n";
    out << "    \"openedHoleCount\": "
        << d.simpleRingCompatibilityStats.openedHoleCount << ",\n";
    out << "    \"openingCutCells\": "
        << d.simpleRingCompatibilityStats.openingCutCells << ",\n";
    out << "    \"openingAttributeCells\": "
        << d.simpleRingCompatibilityStats.openingAttributeCells << ",\n";
    out << "    \"openingRejectedComponents\": "
        << d.simpleRingCompatibilityStats.openingRejectedComponents << ",\n";
    out << "    \"remainingHoleCount\": "
        << d.simpleRingCompatibilityStats.remainingHoleCount << ",\n";
    out << "    \"remainingHoleCells\": "
        << d.simpleRingCompatibilityStats.remainingHoleCells << "\n";
    out << "  },\n";
    out << "  \"weakTailEndpointPortShadow\": {\n";
    out << "    \"evaluated\": "
        << (d.weakTailEndpointPortShadowEvaluated ? "true" : "false") << ",\n";
    out << "    \"componentCount\": "
        << d.weakTailEndpointPortShadowStats.componentCount << ",\n";
    out << "    \"directContactRejected\": "
        << d.weakTailEndpointPortShadowStats.directContactRejected << ",\n";
    out << "    \"endpointPortRejected\": "
        << d.weakTailEndpointPortShadowStats.endpointPortRejected << ",\n";
    out << "    \"junctionGuardRejected\": "
        << d.weakTailEndpointPortShadowStats.junctionGuardRejected << ",\n";
    out << "    \"ownershipRejected\": "
        << d.weakTailEndpointPortShadowStats.ownershipRejected << ",\n";
    out << "    \"directionRejected\": "
        << d.weakTailEndpointPortShadowStats.directionRejected << ",\n";
    out << "    \"acceptedComponents\": "
        << d.weakTailEndpointPortShadowStats.acceptedComponents << ",\n";
    out << "    \"acceptedCells\": "
        << d.weakTailEndpointPortShadowStats.acceptedCells << "\n";
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
        out << "        \"polygonId\": " << evidence.polygonId << ",\n";
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
    out << "    \"polygonIntersections\": " << d.polygonIntersectionCount << ",\n";
    out << "    \"invalidPolygons\": " << d.invalidPolygonCount << ",\n";
    out << "    \"nonFiniteCoordinates\": " << d.nonFiniteCoordinateCount << ",\n";
    out << "    \"zeroLengthEdges\": " << d.zeroLengthEdgeCount << ",\n";
    out << "    \"duplicateVertexRings\": " << d.duplicateVertexRingCount << ",\n";
    out << "    \"degenerateRings\": " << d.degenerateRingCount << ",\n";
    out << "    \"holesOutsideShell\": " << d.holeOutsideShellCount << ",\n";
    out << "    \"nestedHoles\": " << d.nestedHoleCount << ",\n";
    out << "    \"polygonContainments\": " << d.polygonContainmentCount << ",\n";
    out << "    \"polygonInteriorOverlaps\": " << d.polygonInteriorOverlapCount << "\n";
    out << "  },\n";
    out << "  \"timing\": {\n";
    out << "    \"readMs\": " << d.readMs << ",\n";
    out << "    \"traceMs\": " << d.traceMs << ",\n";
    out << "    \"validationMs\": " << d.traceStages.validationMs << ",\n";
    out << "    \"segmentationMs\": " << d.traceStages.segmentationMs << ",\n";
    out << "    \"preprocessingMs\": " << d.traceStages.preprocessingMs << ",\n";
    out << "    \"ridgeTracingMs\": " << d.traceStages.ridgeTracingMs << ",\n";
    out << "    \"beltAndAntiMergeMs\": " << d.traceStages.beltAndAntiMergeMs << ",\n";
    out << "    \"beltConstructionMs\": " << d.traceStages.beltConstructionMs << ",\n";
    out << "    \"rootAntiMergeMs\": " << d.traceStages.rootAntiMergeMs << ",\n";
    out << "    \"localAntiMergeMs\": " << d.traceStages.localAntiMergeMs << ",\n";
    out << "    \"finalWeakContinuityMs\": "
        << d.traceStages.finalWeakContinuityMs << ",\n";
    out << "    \"finalWeakGraphMs\": " << d.traceStages.finalWeakGraphMs << ",\n";
    out << "    \"finalWeakSearchMs\": " << d.traceStages.finalWeakSearchMs << ",\n";
    out << "    \"finalWeakGateMs\": " << d.traceStages.finalWeakGateMs << ",\n";
    out << "    \"finalWeakGateContextMs\": "
        << d.traceStages.finalWeakGateContextMs << ",\n";
    out << "    \"finalWeakGateOwnershipMs\": "
        << d.traceStages.finalWeakGateOwnershipMs << ",\n";
    out << "    \"finalWeakGateComponentHoleMs\": "
        << d.traceStages.finalWeakGateComponentHoleMs << ",\n";
    out << "    \"finalWeakGateEnvelopeMs\": "
        << d.traceStages.finalWeakGateEnvelopeMs << ",\n";
    out << "    \"finalWeakGateTopologyMs\": "
        << d.traceStages.finalWeakGateTopologyMs << ",\n";
    out << "    \"simpleRingCompatibilityMs\": "
        << d.traceStages.simpleRingCompatibilityMs << ",\n";
    out << "    \"vectorizationMs\": " << d.traceStages.vectorizationMs << ",\n";
    out << "    \"postProcessingMs\": " << d.traceStages.postProcessingMs << ",\n";
    out << "    \"coreTotalMs\": " << d.traceStages.totalMs << ",\n";
    out << "    \"topologyRingValidationMs\": "
        << d.topologyRingValidationMs << ",\n";
    out << "    \"topologySelfIntersectionMs\": "
        << d.topologySelfIntersectionMs << ",\n";
    out << "    \"topologyHoleValidationMs\": "
        << d.topologyHoleValidationMs << ",\n";
    out << "    \"topologyPairConflictMs\": "
        << d.topologyPairConflictMs << ",\n";
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
    for (const SegmentIntersectionDetail& detail : diagnostics.geometryValidityDetails) {
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

void writeWeakContinuityCandidatesCsv(
    const std::string& path,
    const std::vector<WeakContinuityCandidate>& candidates) {
    AtomicOutputFile output(filesystemPath(path), 256 * 1024);
    std::ostream& out = output.stream();
    out << std::setprecision(12);
    out << "candidate_id,stage,accepted,selected_by_global_matching,decision,endpoint_a_x,endpoint_a_y,endpoint_b_x,endpoint_b_y,"
           "endpoint_distance,endpoint_distance_world,boundary_adjusted_distance,angle_a,angle_b,width_a,width_b,local_background,minimum_supported_value,"
           "normalized_cost,support_ratio,mean_support,max_unsupported_run,exact_support_ratio,exact_mean_support,exact_max_unsupported_run,"
           "neighborhood_support_ratio,neighborhood_mean_support,neighborhood_max_unsupported_run,used_neighborhood_support,path_length,path_length_ratio,"
           "finite_background_samples,invalid_background_samples,evaluated_support_pixels,"
           "excluded_skeleton_support_pixels,invalid_path_samples,"
           "max_turn_angle,competition_margin_a,competition_margin_b,added_path_pixels,added_envelope_cells,"
           "contacted_object_count,minimum_third_object_clearance,nearby_third_object_count,spatial_conflict_count,"
           "endpoint_branch_id_a,endpoint_branch_id_b,endpoint_junction_distance_a,endpoint_junction_distance_b,"
           "junction_cluster_id_a,junction_cluster_id_b,junction_arm_count_a,junction_arm_count_b,"
           "junction_confidence_a,junction_confidence_b,junction_attribution_available_a,"
           "junction_attribution_available_b,junction_branch_matched_a,junction_branch_matched_b,"
           "junction_ambiguous_a,junction_ambiguous_b,endpoint_component_holes_a,endpoint_component_holes_b,"
           "merged_component_holes_after,local_holes_before,local_holes_after,path_grid_xy\n";
    for (const WeakContinuityCandidate& candidate : candidates) {
        out << candidate.id << ","
            << candidate.stage << ","
            << (candidate.accepted ? 1 : 0) << ","
            << (candidate.selectedByGlobalMatching ? 1 : 0) << ","
            << candidate.decision << ","
            << candidate.endpointA.x << "," << candidate.endpointA.y << ","
            << candidate.endpointB.x << "," << candidate.endpointB.y << ","
            << candidate.endpointDistance << ","
            << candidate.endpointDistanceWorld << ","
            << candidate.boundaryAdjustedDistance << ","
            << candidate.angleA << "," << candidate.angleB << ","
            << candidate.widthA << "," << candidate.widthB << ","
            << candidate.localBackground << ","
            << candidate.minimumSupportedValue << ","
            << candidate.normalizedCost << ","
            << candidate.supportRatio << ","
            << candidate.meanSupport << ","
            << candidate.maxUnsupportedRun << ","
            << candidate.exactSupportRatio << ","
            << candidate.exactMeanSupport << ","
            << candidate.exactMaxUnsupportedRun << ","
            << candidate.neighborhoodSupportRatio << ","
            << candidate.neighborhoodMeanSupport << ","
            << candidate.neighborhoodMaxUnsupportedRun << ","
            << (candidate.usedNeighborhoodSupport ? 1 : 0) << ","
            << candidate.pathLength << ","
            << candidate.pathLengthRatio << ","
            << candidate.finiteBackgroundSamples << ","
            << candidate.invalidBackgroundSamples << ","
            << candidate.evaluatedSupportPixels << ","
            << candidate.excludedSkeletonSupportPixels << ","
            << candidate.invalidPathSamples << ","
            << candidate.maxTurnAngleDegrees << ","
            << candidate.competitionMarginA << ","
            << candidate.competitionMarginB << ","
            << candidate.addedPathPixels << ","
            << candidate.addedEnvelopeCells << ","
            << candidate.contactedObjectCount << ","
            << candidate.minimumThirdObjectClearance << ","
            << candidate.nearbyThirdObjectCount << ","
            << candidate.spatialConflictCount << ","
            << candidate.endpointBranchIdA << ","
            << candidate.endpointBranchIdB << ","
            << candidate.endpointJunctionDistanceA << ","
            << candidate.endpointJunctionDistanceB << ","
            << candidate.junctionClusterIdA << ","
            << candidate.junctionClusterIdB << ","
            << candidate.junctionArmCountA << ","
            << candidate.junctionArmCountB << ","
            << candidate.junctionConfidenceA << ","
            << candidate.junctionConfidenceB << ","
            << (candidate.junctionAttributionAvailableA ? 1 : 0) << ","
            << (candidate.junctionAttributionAvailableB ? 1 : 0) << ","
            << (candidate.junctionBranchMatchedA ? 1 : 0) << ","
            << (candidate.junctionBranchMatchedB ? 1 : 0) << ","
            << (candidate.junctionAmbiguousA ? 1 : 0) << ","
            << (candidate.junctionAmbiguousB ? 1 : 0) << ","
            << candidate.endpointComponentHolesA << ","
            << candidate.endpointComponentHolesB << ","
            << candidate.mergedComponentHolesAfter << ","
            << candidate.localHolesBefore << ","
            << candidate.localHolesAfter << ",";
        for (std::size_t index = 0; index < candidate.path.size(); ++index) {
            if (index > 0) {
                out << ";";
            }
            out << candidate.path[index].x << ":" << candidate.path[index].y;
        }
        out << "\n";
    }
    output.commit();
}

void writeSupportEnvelopeDecisionsCsv(
    const std::string& path,
    const std::vector<SupportEnvelopeDecision>& decisions) {
    AtomicOutputFile output(filesystemPath(path), 64 * 1024);
    std::ostream& out = output.stream();
    out << std::setprecision(12);
    out << "component_id,accepted,decision,component_cells,low_cells,low_ratio,"
           "observed_support_components,observed_major_support_components,"
           "candidate_cut_cells,removed_ratio,resulting_components,"
           "resulting_major_components,minimum_resulting_part_physical_aspect\n";
    for (const SupportEnvelopeDecision& decision : decisions) {
        out << decision.componentId << ","
            << (decision.accepted ? 1 : 0) << ","
            << decision.decision << ","
            << decision.componentCells << ","
            << decision.lowCells << ","
            << decision.lowRatio << ","
            << decision.observedSupportComponents << ","
            << decision.observedMajorSupportComponents << ","
            << decision.candidateCutCells << ","
            << decision.removedRatio << ","
            << decision.resultingComponents << ","
            << decision.resultingMajorComponents << ","
            << decision.minimumResultingPartPhysicalAspect << "\n";
    }
    output.commit();
}

void writeWeakContinuityCandidatesWorldCsv(
    const std::string& path,
    const std::vector<WeakContinuityCandidate>& candidates,
    const Grid2D& grid) {
    AtomicOutputFile output(filesystemPath(path), 512 * 1024);
    std::ostream& out = output.stream();
    const auto sampleWorldX = [&grid](double gridX) {
        return grid.sx() + (gridX + 0.5) * grid.dx();
    };
    const auto sampleWorldY = [&grid](double gridY) {
        return grid.sy() + (gridY + 0.5) * grid.dy();
    };
    out << std::setprecision(12);
    out << "candidate_id,stage,accepted,selected_by_global_matching,decision,"
           "endpoint_a_grid_x,endpoint_a_grid_y,endpoint_b_grid_x,endpoint_b_grid_y,"
           "center_grid_x,center_grid_y,"
           "endpoint_a_world_x,endpoint_a_world_y,endpoint_b_world_x,endpoint_b_world_y,"
           "center_world_x,center_world_y,"
           "endpoint_distance,endpoint_distance_world,boundary_adjusted_distance,angle_a,angle_b,width_a,width_b,local_background,minimum_supported_value,"
           "normalized_cost,support_ratio,mean_support,max_unsupported_run,exact_support_ratio,exact_mean_support,exact_max_unsupported_run,"
           "neighborhood_support_ratio,neighborhood_mean_support,neighborhood_max_unsupported_run,used_neighborhood_support,path_length,path_length_ratio,"
           "finite_background_samples,invalid_background_samples,evaluated_support_pixels,"
           "excluded_skeleton_support_pixels,invalid_path_samples,"
           "max_turn_angle,competition_margin_a,competition_margin_b,added_path_pixels,added_envelope_cells,"
           "contacted_object_count,minimum_third_object_clearance,nearby_third_object_count,spatial_conflict_count,"
           "endpoint_branch_id_a,endpoint_branch_id_b,endpoint_junction_distance_a,endpoint_junction_distance_b,"
           "junction_cluster_id_a,junction_cluster_id_b,junction_arm_count_a,junction_arm_count_b,"
           "junction_confidence_a,junction_confidence_b,junction_attribution_available_a,"
           "junction_attribution_available_b,junction_branch_matched_a,junction_branch_matched_b,"
           "junction_ambiguous_a,junction_ambiguous_b,endpoint_component_holes_a,endpoint_component_holes_b,"
           "merged_component_holes_after,local_holes_before,local_holes_after,path_grid_xy,path_world_xy\n";
    for (const WeakContinuityCandidate& candidate : candidates) {
        double centerGridX = 0.5 * (candidate.endpointA.x + candidate.endpointB.x);
        double centerGridY = 0.5 * (candidate.endpointA.y + candidate.endpointB.y);
        if (!candidate.path.empty()) {
            double sumX = 0.0;
            double sumY = 0.0;
            for (const SkeletonPoint& point : candidate.path) {
                sumX += point.x;
                sumY += point.y;
            }
            centerGridX = sumX / static_cast<double>(candidate.path.size());
            centerGridY = sumY / static_cast<double>(candidate.path.size());
        }

        out << candidate.id << ","
            << candidate.stage << ","
            << (candidate.accepted ? 1 : 0) << ","
            << (candidate.selectedByGlobalMatching ? 1 : 0) << ","
            << candidate.decision << ","
            << candidate.endpointA.x << "," << candidate.endpointA.y << ","
            << candidate.endpointB.x << "," << candidate.endpointB.y << ","
            << centerGridX << "," << centerGridY << ","
            << sampleWorldX(candidate.endpointA.x) << ","
            << sampleWorldY(candidate.endpointA.y) << ","
            << sampleWorldX(candidate.endpointB.x) << ","
            << sampleWorldY(candidate.endpointB.y) << ","
            << sampleWorldX(centerGridX) << ","
            << sampleWorldY(centerGridY) << ","
            << candidate.endpointDistance << ","
            << candidate.endpointDistanceWorld << ","
            << candidate.boundaryAdjustedDistance << ","
            << candidate.angleA << "," << candidate.angleB << ","
            << candidate.widthA << "," << candidate.widthB << ","
            << candidate.localBackground << ","
            << candidate.minimumSupportedValue << ","
            << candidate.normalizedCost << ","
            << candidate.supportRatio << ","
            << candidate.meanSupport << ","
            << candidate.maxUnsupportedRun << ","
            << candidate.exactSupportRatio << ","
            << candidate.exactMeanSupport << ","
            << candidate.exactMaxUnsupportedRun << ","
            << candidate.neighborhoodSupportRatio << ","
            << candidate.neighborhoodMeanSupport << ","
            << candidate.neighborhoodMaxUnsupportedRun << ","
            << (candidate.usedNeighborhoodSupport ? 1 : 0) << ","
            << candidate.pathLength << ","
            << candidate.pathLengthRatio << ","
            << candidate.finiteBackgroundSamples << ","
            << candidate.invalidBackgroundSamples << ","
            << candidate.evaluatedSupportPixels << ","
            << candidate.excludedSkeletonSupportPixels << ","
            << candidate.invalidPathSamples << ","
            << candidate.maxTurnAngleDegrees << ","
            << candidate.competitionMarginA << ","
            << candidate.competitionMarginB << ","
            << candidate.addedPathPixels << ","
            << candidate.addedEnvelopeCells << ","
            << candidate.contactedObjectCount << ","
            << candidate.minimumThirdObjectClearance << ","
            << candidate.nearbyThirdObjectCount << ","
            << candidate.spatialConflictCount << ","
            << candidate.endpointBranchIdA << ","
            << candidate.endpointBranchIdB << ","
            << candidate.endpointJunctionDistanceA << ","
            << candidate.endpointJunctionDistanceB << ","
            << candidate.junctionClusterIdA << ","
            << candidate.junctionClusterIdB << ","
            << candidate.junctionArmCountA << ","
            << candidate.junctionArmCountB << ","
            << candidate.junctionConfidenceA << ","
            << candidate.junctionConfidenceB << ","
            << (candidate.junctionAttributionAvailableA ? 1 : 0) << ","
            << (candidate.junctionAttributionAvailableB ? 1 : 0) << ","
            << (candidate.junctionBranchMatchedA ? 1 : 0) << ","
            << (candidate.junctionBranchMatchedB ? 1 : 0) << ","
            << (candidate.junctionAmbiguousA ? 1 : 0) << ","
            << (candidate.junctionAmbiguousB ? 1 : 0) << ","
            << candidate.endpointComponentHolesA << ","
            << candidate.endpointComponentHolesB << ","
            << candidate.mergedComponentHolesAfter << ","
            << candidate.localHolesBefore << ","
            << candidate.localHolesAfter << ",";
        for (std::size_t index = 0; index < candidate.path.size(); ++index) {
            if (index > 0) {
                out << ";";
            }
            out << candidate.path[index].x << ":" << candidate.path[index].y;
        }
        out << ",";
        for (std::size_t index = 0; index < candidate.path.size(); ++index) {
            if (index > 0) {
                out << ";";
            }
            out << sampleWorldX(candidate.path[index].x) << ":"
                << sampleWorldY(candidate.path[index].y);
        }
        out << "\n";
    }
    output.commit();
}

void writeWeakTailCandidatesCsv(
    const std::string& path,
    const std::vector<WeakTailCandidate>& candidates) {
    AtomicOutputFile output(filesystemPath(path), 256 * 1024);
    std::ostream& out = output.stream();
    out << std::setprecision(12);
    out << "candidate_id,accepted,decision,cell_count,min_x,min_y,max_x,max_y,"
           "owner_label,touched_owner_count,direct_owner_count,direct_contact_count,"
           "endpoint_port_count,endpoint_x,endpoint_y,intersects_junction_guard,"
           "span,aspect,mean_value,max_value,"
           "alignment_angle,topology_gate_evaluated,new_enclosures_before,"
           "new_enclosures_without_candidate,new_enclosure_cells_before,"
           "new_enclosure_cells_without_candidate,topology_trimmed_cells,cells_grid_xy\n";
    for (const WeakTailCandidate& candidate : candidates) {
        out << candidate.id << ","
            << (candidate.accepted ? 1 : 0) << ","
            << candidate.decision << ","
            << candidate.cellCount << ","
            << candidate.minX << "," << candidate.minY << ","
            << candidate.maxX << "," << candidate.maxY << ","
            << candidate.ownerLabel << ","
            << candidate.touchedOwnerCount << ","
            << candidate.directOwnerCount << ","
            << candidate.directContactCount << ","
            << candidate.endpointPortCount << ","
            << candidate.endpointX << ","
            << candidate.endpointY << ","
            << (candidate.intersectsJunctionGuard ? 1 : 0) << ","
            << candidate.span << ","
            << candidate.aspect << ","
            << candidate.meanValue << ","
            << candidate.maxValue << ","
            << candidate.alignmentAngleDegrees << ","
            << (candidate.topologyGateEvaluated ? 1 : 0) << ","
            << candidate.newEnclosuresBefore << ","
            << candidate.newEnclosuresWithoutCandidate << ","
            << candidate.newEnclosureCellsBefore << ","
            << candidate.newEnclosureCellsWithoutCandidate << ","
            << candidate.topologyTrimmedCells << ",";
        for (std::size_t index = 0; index < candidate.cells.size(); ++index) {
            if (index > 0) {
                out << ";";
            }
            out << candidate.cells[index].x << ":" << candidate.cells[index].y;
        }
        out << "\n";
    }
    output.commit();
}

void writeWeakExtensionGateEvidenceCsv(
    const std::string& path,
    const std::vector<WeakExtensionGateEvidence>& evidence) {
    AtomicOutputFile output(filesystemPath(path), 256 * 1024);
    std::ostream& out = output.stream();
    out << std::setprecision(12);
    out << "component_id,accepted,decision,small_component,cell_count,owner_count,"
           "finite_value_count,invalid_value_count,physical_span,physical_aspect,"
           "estimated_width_world,support_ratio,mean_support\n";
    for (const WeakExtensionGateEvidence& item : evidence) {
        out << item.id << ","
            << (item.accepted ? 1 : 0) << ","
            << item.decision << ","
            << (item.smallComponent ? 1 : 0) << ","
            << item.cellCount << ","
            << item.ownerCount << ","
            << item.finiteValueCount << ","
            << item.invalidValueCount << ","
            << item.physicalSpan << ","
            << item.physicalAspect << ","
            << item.estimatedWidthWorld << ","
            << item.supportRatio << ","
            << item.meanSupport << "\n";
    }
    output.commit();
}

} // namespace fpt
