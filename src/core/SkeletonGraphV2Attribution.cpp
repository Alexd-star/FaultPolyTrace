#include "SkeletonGraphV2Attribution.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fpt {
namespace {

const SkeletonNodeV2* nodeById(const SkeletonGraphV2& graph, int id) {
    for (const SkeletonNodeV2& node : graph.nodes) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

bool isJunctionNode(const SkeletonNodeV2* node) {
    return node && node->type == SkeletonNodeTypeV2::JunctionCluster;
}

std::vector<double> sortedTurnAngles(const JunctionAttributionResult& junction) {
    std::vector<double> angles;
    angles.reserve(junction.pairs.size());
    for (const BranchAttributionPair& pair : junction.pairs) {
        angles.push_back(pair.turnAngleDegrees);
    }
    std::sort(angles.begin(), angles.end());
    return angles;
}

bool equivalentJunctionAttribution(
    const JunctionAttributionResult& legacy,
    const JunctionAttributionResult& candidate) {
    if (legacy.ambiguous != candidate.ambiguous ||
        legacy.pairs.size() != candidate.pairs.size() ||
        legacy.unmatchedBranchIds.size() != candidate.unmatchedBranchIds.size()) {
        return false;
    }
    const std::vector<double> legacyAngles = sortedTurnAngles(legacy);
    const std::vector<double> candidateAngles = sortedTurnAngles(candidate);
    for (std::size_t index = 0; index < legacyAngles.size(); ++index) {
        if (std::abs(legacyAngles[index] - candidateAngles[index]) > 5.0) {
            return false;
        }
    }
    return true;
}

} // namespace

SkeletonGraphV2AttributionComparison compareSkeletonGraphV2Attribution(
    const Grid2D& grid,
    const std::vector<const SkeletonBranch*>& legacyBranches,
    const SkeletonGraphV2& graphV2,
    const BranchAttributionOptions& options) {
    SkeletonGraphV2AttributionComparison result;
    result.evaluated = true;
    result.legacyBranchCount = static_cast<int>(legacyBranches.size());
    result.v2BranchCount = static_cast<int>(graphV2.branches.size());
    result.v2AmbiguousPortAttachments = graphV2.stats.ambiguousPortAttachments;
    result.legacy = analyzeBranchAttribution(grid, legacyBranches, options);
    result.legacyJunctionClusters = result.legacy.junctionClusterCount;
    result.legacyAcceptedPairs = result.legacy.acceptedPairCount;
    result.legacyAmbiguousClusters = result.legacy.ambiguousClusterCount;
    result.legacyUnmatchedEnds = result.legacy.unmatchedEndCount;

    std::vector<SkeletonBranch> converted;
    converted.reserve(graphV2.branches.size());
    for (const SkeletonBranchV2& branch : graphV2.branches) {
        if (branch.points.size() < 2 || branch.closedLoop) {
            if (branch.closedLoop) {
                ++result.skippedClosedLoopBranches;
            } else {
                ++result.invalidBranches;
            }
            continue;
        }
        const SkeletonNodeV2* startNode = nodeById(graphV2, branch.startNodeId);
        const SkeletonNodeV2* endNode = nodeById(graphV2, branch.endNodeId);
        if (!startNode || !endNode) {
            ++result.invalidBranches;
            continue;
        }
        SkeletonBranch convertedBranch;
        convertedBranch.id = branch.id;
        convertedBranch.points = branch.points;
        convertedBranch.startIsJunction = isJunctionNode(startNode);
        convertedBranch.endIsJunction = isJunctionNode(endNode);
        if (convertedBranch.startIsJunction) {
            const SkeletonPoint center{
                static_cast<int>(std::llround(startNode->centerGridX)),
                static_cast<int>(std::llround(startNode->centerGridY))};
            if (convertedBranch.points.empty() ||
                convertedBranch.points.front().x != center.x ||
                convertedBranch.points.front().y != center.y) {
                convertedBranch.points.insert(
                    convertedBranch.points.begin(), center);
            }
        }
        if (convertedBranch.endIsJunction) {
            const SkeletonPoint center{
                static_cast<int>(std::llround(endNode->centerGridX)),
                static_cast<int>(std::llround(endNode->centerGridY))};
            if (convertedBranch.points.empty() ||
                convertedBranch.points.back().x != center.x ||
                convertedBranch.points.back().y != center.y) {
                convertedBranch.points.push_back(center);
            }
        }
        converted.push_back(std::move(convertedBranch));
    }
    std::vector<const SkeletonBranch*> convertedRefs;
    convertedRefs.reserve(converted.size());
    for (const SkeletonBranch& branch : converted) {
        convertedRefs.push_back(&branch);
    }
    result.convertedBranchCount = static_cast<int>(convertedRefs.size());
    result.v2 = analyzeBranchAttribution(grid, convertedRefs, options);
    result.v2JunctionClusters = result.v2.junctionClusterCount;
    result.v2AcceptedPairs = result.v2.acceptedPairCount;
    result.v2AmbiguousClusters = result.v2.ambiguousClusterCount;
    result.v2UnmatchedEnds = result.v2.unmatchedEndCount;
    result.acceptedPairCountDelta = result.v2AcceptedPairs - result.legacyAcceptedPairs;
    result.ambiguousClusterCountDelta = result.v2AmbiguousClusters - result.legacyAmbiguousClusters;
    result.unmatchedEndCountDelta = result.v2UnmatchedEnds - result.legacyUnmatchedEnds;
    result.branchCountDelta = result.convertedBranchCount - result.legacyBranchCount;
    result.junctionClusterCountDelta = result.v2JunctionClusters - result.legacyJunctionClusters;

    std::vector<unsigned char> matchedV2(result.v2.junctions.size(), 0);
    double matchedDistanceSum = 0.0;
    const double maximumCenterDistance = std::max(
        2.5, 2.0 * static_cast<double>(options.junctionClusterRadius) + 1.0);
    for (const JunctionAttributionResult& legacyJunction : result.legacy.junctions) {
        int bestIndex = -1;
        double bestDistance = maximumCenterDistance + 1.0;
        for (std::size_t index = 0; index < result.v2.junctions.size(); ++index) {
            if (matchedV2[index]) {
                continue;
            }
            const JunctionAttributionResult& v2Junction = result.v2.junctions[index];
            const double distance = std::hypot(
                legacyJunction.centerGridX - v2Junction.centerGridX,
                legacyJunction.centerGridY - v2Junction.centerGridY);
            if (distance < bestDistance - 1e-12 ||
                (std::abs(distance - bestDistance) <= 1e-12 &&
                 (bestIndex < 0 || static_cast<int>(index) < bestIndex))) {
                bestDistance = distance;
                bestIndex = static_cast<int>(index);
            }
        }
        if (bestIndex < 0 || bestDistance > maximumCenterDistance) {
            ++result.legacyOnlyJunctionClusters;
            continue;
        }
        matchedV2[static_cast<std::size_t>(bestIndex)] = 1;
        ++result.matchedJunctionClusters;
        matchedDistanceSum += bestDistance;
        result.maximumMatchedCenterDistanceCells = std::max(
            result.maximumMatchedCenterDistanceCells, bestDistance);
        const JunctionAttributionResult& v2Junction =
            result.v2.junctions[static_cast<std::size_t>(bestIndex)];
        if (equivalentJunctionAttribution(legacyJunction, v2Junction)) {
            ++result.equivalentMatchedJunctionClusters;
        } else {
            ++result.differingMatchedJunctionClusters;
        }
        if (legacyJunction.ambiguous && !v2Junction.ambiguous) {
            ++result.ambiguityImprovedJunctionClusters;
        } else if (!legacyJunction.ambiguous && v2Junction.ambiguous) {
            ++result.ambiguityRegressedJunctionClusters;
        }
    }
    result.v2OnlyJunctionClusters = static_cast<int>(std::count(
        matchedV2.begin(), matchedV2.end(), static_cast<unsigned char>(0)));
    if (result.matchedJunctionClusters > 0) {
        result.meanMatchedCenterDistanceCells = matchedDistanceSum /
            static_cast<double>(result.matchedJunctionClusters);
    }
    return result;
}

} // namespace fpt
