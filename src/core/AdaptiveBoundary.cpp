#include "AdaptiveBoundary.h"

#include "PolygonQualityMetrics.h"
#include "Topology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <unordered_map>

namespace fpt {
namespace {

double signedArea(const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Point& a = ring[i];
        const Point& b = ring[(i + 1) % ring.size()];
        sum += a.x * b.y - b.x * a.y;
    }
    return 0.5 * sum;
}

bool supportedAtWorld(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const Point& world,
    double threshold,
    bool& finite) {
    const int x = static_cast<int>(std::llround((world.x - grid.sx()) / grid.dx() - 0.5));
    const int y = static_cast<int>(std::llround((world.y - grid.sy()) / grid.dy() - 0.5));
    if (!grid.inBounds(x, y)) {
        finite = false;
        return false;
    }
    const double value = grid.value(x, y);
    finite = std::isfinite(value);
    return finite && supportMask.get(x, y) && value >= threshold;
}

double boundaryTransitionScore(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const Point& boundary,
    double exteriorNormalX,
    double exteriorNormalY,
    double threshold) {
    const double spacing = std::min(grid.dx(), grid.dy());
    double score = 0.0;
    int samples = 0;
    for (int sample = 0; sample < 3; ++sample) {
        const double distance = (0.5 + static_cast<double>(sample)) * spacing;
        const Point inward{
            boundary.x - exteriorNormalX * distance,
            boundary.y - exteriorNormalY * distance};
        const Point outward{
            boundary.x + exteriorNormalX * distance,
            boundary.y + exteriorNormalY * distance};
        bool inwardFinite = false;
        bool outwardFinite = false;
        const bool inwardSupported = supportedAtWorld(
            grid, supportMask, inward, threshold, inwardFinite);
        const bool outwardSupported = supportedAtWorld(
            grid, supportMask, outward, threshold, outwardFinite);
        if (inwardFinite) {
            score += inwardSupported ? 1.0 : 0.0;
            ++samples;
        }
        if (outwardFinite) {
            score += outwardSupported ? 0.0 : 1.0;
            ++samples;
        }
    }
    return samples > 0 ? score / static_cast<double>(samples) : 0.0;
}

struct EvidenceConfidenceIndex {
    int width = 0;
    int height = 0;
    std::vector<double> confidence;

    double at(const Grid2D& grid, const Point& world) const {
        if (confidence.empty()) {
            return 1.0;
        }
        const int x = static_cast<int>(std::llround(
            (world.x - grid.sx()) / grid.dx() - 0.5));
        const int y = static_cast<int>(std::llround(
            (world.y - grid.sy()) / grid.dy() - 0.5));
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return 0.0;
        }
        return confidence[static_cast<std::size_t>(y * width + x)];
    }
};

EvidenceConfidenceIndex buildEvidenceConfidenceIndex(
    const MultiScaleEvidenceResult* evidence,
    const Grid2D& grid,
    std::size_t maximumSamples) {
    EvidenceConfidenceIndex index;
    index.width = grid.nx();
    index.height = grid.ny();
    if (!evidence || evidence->samples.empty()) {
        return index;
    }
    index.confidence.assign(grid.size(), 0.0);
    const std::size_t limit = std::min(maximumSamples, evidence->samples.size());
    for (std::size_t sampleIndex = 0; sampleIndex < limit; ++sampleIndex) {
        const MultiScaleEvidenceSample& sample = evidence->samples[sampleIndex];
        if (!sample.finite || !grid.inBounds(sample.x, sample.y) ||
            !std::isfinite(sample.confidence)) {
            continue;
        }
        double& stored = index.confidence[static_cast<std::size_t>(
            sample.y * grid.nx() + sample.x)];
        stored = std::max(stored, std::max(0.0, std::min(1.0, sample.confidence)));
    }
    return index;
}

AdaptiveBoundaryEvidence evaluateOne(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const Polygon& polygon,
    const std::vector<Polygon>& allPolygons,
    const EvidenceConfidenceIndex& auxiliaryEvidence,
    const AdaptiveBoundaryOptions& options,
    const BinaryMask* weakSupportMask) {
    AdaptiveBoundaryEvidence evidence;
    evidence.polygonId = polygon.id;
    evidence.candidate = polygon;
    Polygon originalGeometry = polygon;
    refreshPolygonGeometry(originalGeometry);
    evidence.originalArea = originalGeometry.area;
    evidence.candidateArea = evidence.originalArea;
    evidence.originalRoughness = polygonBoundaryRoughness(polygon);
    if (polygon.points.size() < 3) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "ring_too_short";
        evidence.candidateFinite = false;
        return evidence;
    }
    if (polygon.points.size() > options.maximumVerticesPerPolygon) {
        evidence.decision = AdaptiveBoundaryDecision::Review;
        evidence.reason = "vertex_budget";
        return evidence;
    }
    const double step = options.searchStepWorld > 0.0
        ? options.searchStepWorld : std::min(grid.dx(), grid.dy()) * 0.5;
    const int steps = std::max(1, static_cast<int>(std::floor(
        options.maximumDisplacementWorld / step + 1e-9)));
    const double orientation = signedArea(polygon.points) >= 0.0 ? 1.0 : -1.0;
    std::vector<double> rawOffsets(polygon.points.size(), 0.0);
    std::vector<double> rawScoreGains(polygon.points.size(), 0.0);
    std::vector<Point> normals(polygon.points.size());
    for (std::size_t index = 0; index < polygon.points.size(); ++index) {
        const Point& previous = polygon.points[(index + polygon.points.size() - 1) % polygon.points.size()];
        const Point& current = polygon.points[index];
        const Point& next = polygon.points[(index + 1) % polygon.points.size()];
        const double tangentX = next.x - previous.x;
        const double tangentY = next.y - previous.y;
        const double tangentLength = std::hypot(tangentX, tangentY);
        if (tangentLength <= 1e-12) {
            evidence.candidateFinite = false;
            continue;
        }
        // For a counter-clockwise ring the exterior is on the right side.
        const double normalX = orientation * tangentY / tangentLength;
        const double normalY = orientation * -tangentX / tangentLength;
        normals[index] = {normalX, normalY};
        const double originalTransition = boundaryTransitionScore(
            grid, supportMask, current, normalX, normalY,
            options.supportThreshold);
        const double originalScore = originalTransition +
            0.05 * auxiliaryEvidence.at(grid, current);
        double bestScore = originalScore;
        double bestOffset = 0.0;
        for (int direction = -1; direction <= 1; direction += 2) {
            for (int stepIndex = 1; stepIndex <= steps; ++stepIndex) {
                const double offset = static_cast<double>(direction * stepIndex) * step;
                const Point candidatePoint{
                    current.x + normalX * offset,
                    current.y + normalY * offset};
                const double transition = boundaryTransitionScore(
                    grid, supportMask, candidatePoint, normalX, normalY,
                    options.supportThreshold);
                const double confidence = auxiliaryEvidence.at(grid, candidatePoint);
                const double score = transition + 0.05 * confidence -
                    0.02 * std::abs(offset) / std::max(step, 1e-9);
                if (score > bestScore + options.minimumSupportGain) {
                    bestScore = score;
                    bestOffset = offset;
                }
            }
        }
        rawOffsets[index] = bestOffset;
        rawScoreGains[index] = std::max(0.0, bestScore - originalScore);
    }
    std::vector<double> regularizedOffsets(rawOffsets.size(), 0.0);
    for (std::size_t index = 0; index < rawOffsets.size(); ++index) {
        const std::size_t previous =
            (index + rawOffsets.size() - 1) % rawOffsets.size();
        const std::size_t next = (index + 1) % rawOffsets.size();
        regularizedOffsets[index] =
            (1.0 - options.offsetRegularization) * rawOffsets[index] +
            options.offsetRegularization * 0.5 *
                (rawOffsets[previous] + rawOffsets[next]);
        regularizedOffsets[index] = std::max(
            -options.maximumDisplacementWorld,
            std::min(options.maximumDisplacementWorld,
                     regularizedOffsets[index]));
    }
    for (std::size_t index = 0; index < regularizedOffsets.size(); ++index) {
        const double offset = regularizedOffsets[index];
        if (std::abs(offset) <= 1e-12) {
            continue;
        }
        Point& destination = evidence.candidate.points[index];
        destination.x += normals[index].x * offset;
        destination.y += normals[index].y * offset;
        ++evidence.movedVertices;
        evidence.maximumDisplacementWorld = std::max(
            evidence.maximumDisplacementWorld, std::abs(offset));
    }
    double scoreGainSum = 0.0;
    int scoreGainCount = 0;
    for (std::size_t index = 0; index < rawOffsets.size(); ++index) {
        if (std::abs(rawOffsets[index]) <= 1e-12) {
            continue;
        }
        scoreGainSum += rawScoreGains[index];
        ++scoreGainCount;
    }
    if (scoreGainCount > 0) {
        evidence.boundaryScoreGain = scoreGainSum /
            static_cast<double>(scoreGainCount);
    }
    evidence.candidateChanged = evidence.movedVertices > 0;
    refreshPolygonGeometry(evidence.candidate);
    evidence.candidateArea = evidence.candidate.area;
    evidence.candidateRoughness =
        polygonBoundaryRoughness(evidence.candidate);
    if (evidence.originalRoughness > 1e-18) {
        evidence.roughnessChangeRatio =
            (evidence.candidateRoughness - evidence.originalRoughness) /
            evidence.originalRoughness;
    } else if (evidence.candidateRoughness > 1e-18) {
        evidence.roughnessChangeRatio = 1.0;
    }
    for (const Point& point : evidence.candidate.points) {
        evidence.candidateFinite = evidence.candidateFinite &&
            std::isfinite(point.x) && std::isfinite(point.y);
    }
    for (const std::vector<Point>& hole : evidence.candidate.holes) {
        for (const Point& point : hole) {
            evidence.candidateFinite = evidence.candidateFinite &&
                std::isfinite(point.x) && std::isfinite(point.y);
        }
    }
    if (!evidence.candidateFinite) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "non_finite_or_degenerate_candidate";
        return evidence;
    }
    if (!evidence.candidateChanged) {
        evidence.decision = AdaptiveBoundaryDecision::NoChange;
        evidence.reason = "no_supported_boundary_improvement";
        return evidence;
    }
    if (!std::isfinite(evidence.candidateArea) || evidence.candidateArea <= 1e-9) {
        evidence.candidateFinite = false;
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "invalid_candidate_area";
        return evidence;
    }
    evidence.areaChangeRatio = std::abs(
        evidence.candidateArea - evidence.originalArea) /
        std::max(evidence.originalArea, 1e-9);
    evidence.topologyValid = !polygonHasTopologyConflict(
        evidence.candidate, allPolygons, true);
    if (!evidence.topologyValid) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "topology_conflict";
        return evidence;
    }
    const BinaryMask& weakMask = weakSupportMask
        ? *weakSupportMask
        : supportMask;
    const double weakThreshold = options.weakSupportThreshold > 0.0
        ? options.weakSupportThreshold
        : options.supportThreshold;
    const PolygonDualSupportQuality original =
        measurePolygonDualSupportQuality(
            grid, supportMask, weakMask, polygon,
            options.supportThreshold, weakThreshold,
            options.maximumRasterCellsPerPolygon);
    const PolygonDualSupportQuality candidate =
        measurePolygonDualSupportQuality(
            grid, supportMask, weakMask, evidence.candidate,
            options.supportThreshold, weakThreshold,
            options.maximumRasterCellsPerPolygon);
    evidence.rasterCellsEvaluated =
        original.evaluatedCells + candidate.evaluatedCells;
    evidence.rasterBudgetExceeded =
        original.budgetExceeded || candidate.budgetExceeded;
    if (evidence.rasterBudgetExceeded) {
        evidence.decision = AdaptiveBoundaryDecision::Review;
        evidence.reason = "raster_budget";
        return evidence;
    }
    if (original.insideCells > 0) {
        evidence.supportGainRatio = static_cast<double>(
            candidate.strongSupportedCells - original.strongSupportedCells) /
            static_cast<double>(std::max(1, original.strongSupportedCells));
        evidence.supportLossRatio = static_cast<double>(std::max(
            0,
            original.strongSupportedCells -
                candidate.strongSupportedCells)) /
            static_cast<double>(
                original.strongSupportedCells > 0
                    ? original.strongSupportedCells
                    : 1);
    }
    if (candidate.insideCells > 0) {
        evidence.backgroundLeakageIncreaseRatio = static_cast<double>(
            std::max(
                0,
                candidate.backgroundCells - original.backgroundCells)) /
            static_cast<double>(candidate.insideCells);
    }
    if (original.weakSupportedCells > 0) {
        evidence.weakSupportGainRatio = static_cast<double>(
            candidate.weakSupportedCells - original.weakSupportedCells) /
            static_cast<double>(original.weakSupportedCells);
        evidence.weakSupportLossRatio = static_cast<double>(std::max(
            0,
            original.weakSupportedCells - candidate.weakSupportedCells)) /
            static_cast<double>(original.weakSupportedCells);
    }
    if (evidence.areaChangeRatio > options.maximumAreaChangeRatio) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "area_change";
    } else if (evidence.supportLossRatio > options.maximumSupportLossRatio) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "support_loss";
    } else if (evidence.weakSupportLossRatio >
               options.maximumWeakSupportLossRatio) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "weak_support_loss";
    } else if (evidence.backgroundLeakageIncreaseRatio >
               options.maximumBackgroundLeakageIncreaseRatio) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "background_leakage";
    } else if (evidence.roughnessChangeRatio >
               options.maximumRoughnessIncreaseRatio) {
        evidence.decision = AdaptiveBoundaryDecision::Rejected;
        evidence.reason = "roughness_increase";
    } else if (evidence.boundaryScoreGain > options.minimumSupportGain) {
        evidence.decision = AdaptiveBoundaryDecision::AcceptedCandidate;
        evidence.reason = "bounded_transition_gain";
    } else {
        evidence.decision = AdaptiveBoundaryDecision::Review;
        evidence.reason = "changed_without_clear_gain";
    }
    return evidence;
}

} // namespace

void validateAdaptiveBoundaryOptions(const AdaptiveBoundaryOptions& options) {
    if (!std::isfinite(options.maximumDisplacementWorld) ||
        options.maximumDisplacementWorld < 0.0 ||
        !std::isfinite(options.searchStepWorld) || options.searchStepWorld < 0.0 ||
        !std::isfinite(options.supportThreshold) ||
        !std::isfinite(options.weakSupportThreshold) ||
        options.weakSupportThreshold < 0.0 ||
        !std::isfinite(options.minimumSupportGain) ||
        options.minimumSupportGain < 0.0 || options.minimumSupportGain > 1.0 ||
        !std::isfinite(options.maximumSupportLossRatio) ||
        options.maximumSupportLossRatio < 0.0 || options.maximumSupportLossRatio > 1.0 ||
        !std::isfinite(options.maximumWeakSupportLossRatio) ||
        options.maximumWeakSupportLossRatio < 0.0 ||
        options.maximumWeakSupportLossRatio > 1.0 ||
        !std::isfinite(options.maximumBackgroundLeakageIncreaseRatio) ||
        options.maximumBackgroundLeakageIncreaseRatio < 0.0 ||
        options.maximumBackgroundLeakageIncreaseRatio > 1.0 ||
        !std::isfinite(options.maximumAreaChangeRatio) ||
        options.maximumAreaChangeRatio < 0.0 ||
        options.maximumAreaChangeRatio > 1.0 ||
        !std::isfinite(options.maximumRoughnessIncreaseRatio) ||
        options.maximumRoughnessIncreaseRatio < 0.0 ||
        options.maximumRoughnessIncreaseRatio > 1.0 ||
        !std::isfinite(options.offsetRegularization) ||
        options.offsetRegularization < 0.0 ||
        options.offsetRegularization > 1.0) {
        throw std::runtime_error("Adaptive boundary numeric options are invalid.");
    }
    if (options.localRadiusCells < 0 || options.maximumVerticesPerPolygon == 0 ||
        options.maximumPolygons == 0 || options.maximumRasterCellsPerPolygon == 0) {
        throw std::runtime_error("Adaptive boundary budgets must be positive.");
    }
}

AdaptiveBoundaryResult evaluateAdaptiveBoundaries(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const std::vector<Polygon>& polygons,
    const MultiScaleEvidenceResult* auxiliaryEvidence,
    const AdaptiveBoundaryOptions& options,
    const BinaryMask* weakSupportMask) {
    validateAdaptiveBoundaryOptions(options);
    if (supportMask.width() != grid.nx() || supportMask.height() != grid.ny()) {
        throw std::runtime_error("Adaptive boundary support mask dimensions must match the grid.");
    }
    if (weakSupportMask &&
        (weakSupportMask->width() != grid.nx() ||
         weakSupportMask->height() != grid.ny())) {
        throw std::runtime_error(
            "Adaptive boundary weak-support mask dimensions must match the grid.");
    }
    AdaptiveBoundaryResult result;
    const EvidenceConfidenceIndex evidenceIndex = buildEvidenceConfidenceIndex(
        auxiliaryEvidence, grid,
        auxiliaryEvidence ? auxiliaryEvidence->samples.size() : 0U);
    std::vector<std::size_t> keptIndices;
    keptIndices.reserve(std::min(options.maximumPolygons, polygons.size()));
    std::size_t keptPolygonCount = 0;
    for (std::size_t index = 0; index < polygons.size(); ++index) {
        if (!polygons[index].kept) {
            continue;
        }
        ++keptPolygonCount;
        if (keptIndices.size() < options.maximumPolygons) {
            keptIndices.push_back(index);
        }
    }
    result.evidence.reserve(keptIndices.size());
    double gainSum = 0.0;
    double weakGainSum = 0.0;
    double boundaryGainSum = 0.0;
    double leakageSum = 0.0;
    for (const std::size_t index : keptIndices) {
        AdaptiveBoundaryEvidence evidence = evaluateOne(
            grid, supportMask, polygons[index], polygons,
            evidenceIndex, options, weakSupportMask);
        ++result.summary.polygonsEvaluated;
        result.summary.movedVertices += evidence.movedVertices;
        result.summary.maximumDisplacementWorld = std::max(
            result.summary.maximumDisplacementWorld,
            evidence.maximumDisplacementWorld);
        gainSum += evidence.supportGainRatio;
        weakGainSum += evidence.weakSupportGainRatio;
        boundaryGainSum += evidence.boundaryScoreGain;
        leakageSum += evidence.backgroundLeakageIncreaseRatio;
        result.summary.rasterCellsEvaluated += evidence.rasterCellsEvaluated;
        if (!evidence.topologyValid) {
            ++result.summary.topologyRejectedCandidates;
        }
        if (evidence.rasterBudgetExceeded) {
            ++result.summary.rasterBudgetReviewCandidates;
        }
        if (evidence.decision == AdaptiveBoundaryDecision::NoChange) {
            ++result.summary.noChange;
        } else if (evidence.decision == AdaptiveBoundaryDecision::AcceptedCandidate) {
            ++result.summary.acceptedCandidates;
        } else if (evidence.decision == AdaptiveBoundaryDecision::Review) {
            ++result.summary.reviewCandidates;
        } else {
            ++result.summary.rejectedCandidates;
        }
        result.evidence.push_back(std::move(evidence));
    }
    result.summary.truncatedPolygons = static_cast<int>(
        keptPolygonCount - keptIndices.size());
    if (result.summary.polygonsEvaluated > 0) {
        result.summary.meanSupportGainRatio = gainSum /
            static_cast<double>(result.summary.polygonsEvaluated);
        result.summary.meanWeakSupportGainRatio = weakGainSum /
            static_cast<double>(result.summary.polygonsEvaluated);
        result.summary.meanBoundaryScoreGain = boundaryGainSum /
            static_cast<double>(result.summary.polygonsEvaluated);
        result.summary.meanBackgroundLeakageIncreaseRatio = leakageSum /
            static_cast<double>(result.summary.polygonsEvaluated);
    }
    return result;
}

AdaptiveBoundaryCommitResult commitAdaptiveBoundaryCandidates(
    const std::vector<Polygon>& polygons,
    const std::vector<AdaptiveBoundaryEvidence>& evidence) {
    AdaptiveBoundaryCommitResult result;
    result.polygons = polygons;

    std::unordered_map<int, std::size_t> indexById;
    indexById.reserve(result.polygons.size());
    for (std::size_t index = 0; index < result.polygons.size(); ++index) {
        indexById.emplace(result.polygons[index].id, index);
    }

    std::vector<std::size_t> order;
    order.reserve(evidence.size());
    for (std::size_t index = 0; index < evidence.size(); ++index) {
        if (evidence[index].decision ==
            AdaptiveBoundaryDecision::AcceptedCandidate) {
            order.push_back(index);
        }
    }
    std::stable_sort(
        order.begin(),
        order.end(),
        [&](std::size_t left, std::size_t right) {
            const AdaptiveBoundaryEvidence& first = evidence[left];
            const AdaptiveBoundaryEvidence& second = evidence[right];
            if (std::abs(first.boundaryScoreGain -
                         second.boundaryScoreGain) > 1e-12) {
                return first.boundaryScoreGain > second.boundaryScoreGain;
            }
            if (std::abs(first.roughnessChangeRatio -
                         second.roughnessChangeRatio) > 1e-12) {
                return first.roughnessChangeRatio <
                    second.roughnessChangeRatio;
            }
            return first.polygonId < second.polygonId;
        });

    for (const std::size_t evidenceIndex : order) {
        ++result.summary.attemptedCandidates;
        const AdaptiveBoundaryEvidence& candidateEvidence =
            evidence[evidenceIndex];
        const auto found = indexById.find(candidateEvidence.polygonId);
        if (found == indexById.end()) {
            ++result.summary.missingPolygonCandidates;
            continue;
        }
        Polygon candidate = candidateEvidence.candidate;
        candidate.kept = result.polygons[found->second].kept;
        refreshPolygonGeometry(candidate);
        if (polygonHasTopologyConflict(candidate, result.polygons, true)) {
            ++result.summary.globalConflictRejectedCandidates;
            continue;
        }
        result.polygons[found->second] = std::move(candidate);
        ++result.summary.committedCandidates;
    }

    const TopologyDiagnostics topology =
        diagnoseTopology(result.polygons, true);
    if (!topologyIsValid(topology)) {
        result.polygons = polygons;
        result.summary.committedCandidates = 0;
        result.summary.batchRolledBack = true;
    }
    return result;
}

} // namespace fpt
