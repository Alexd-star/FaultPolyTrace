#pragma once

#include "BinaryMask.h"

#include <vector>

namespace fpt {

struct SkeletonPoint {
    int x = 0;
    int y = 0;
};

struct SkeletonBranch {
    int id = 0;
    std::vector<SkeletonPoint> points;
    bool startIsJunction = false;
    bool endIsJunction = false;
};

struct SkeletonGraph {
    std::vector<SkeletonPoint> endpoints;
    std::vector<SkeletonPoint> junctions;
    std::vector<SkeletonBranch> branches;
};

struct BranchMaskResult {
    BinaryMask mask;
    int keptBranches = 0;
    int prunedBranches = 0;
};

struct GapConnectionResult {
    BinaryMask mask;
    int connectedGaps = 0;
};

using GapSupportPredicate = bool (*)(const SkeletonPoint& a, const SkeletonPoint& b, void* context);

SkeletonGraph buildSkeletonGraph(const BinaryMask& skeleton);
BinaryMask branchesToMask(const SkeletonGraph& graph, int width, int height, int minBranchLength, int minJunctionBranchLength = 2);
BranchMaskResult branchesToMaskWithStats(
    const SkeletonGraph& graph,
    int width,
    int height,
    int minBranchLength,
    int minJunctionBranchLength = 2);
GapConnectionResult connectEndpointGapsWithStats(const BinaryMask& skeleton, const SkeletonGraph& graph, int maxGapDistance, double maxAngleDegrees);
GapConnectionResult connectEndpointGapsWithStats(
    const BinaryMask& skeleton,
    const SkeletonGraph& graph,
    int maxGapDistance,
    double maxAngleDegrees,
    GapSupportPredicate supportPredicate,
    void* supportContext);
BinaryMask connectEndpointGaps(const BinaryMask& skeleton, const SkeletonGraph& graph, int maxGapDistance, double maxAngleDegrees);

} // namespace fpt
