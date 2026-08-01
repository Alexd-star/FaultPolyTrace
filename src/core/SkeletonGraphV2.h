#pragma once

#include "BinaryMask.h"
#include "GridMetric.h"
#include "SkeletonGraph.h"

#include <vector>

namespace fpt {

enum class SkeletonNodeTypeV2 {
    Endpoint,
    JunctionCluster,
    Isolated,
    Loop
};

struct SkeletonNodeV2 {
    int id = 0;
    SkeletonNodeTypeV2 type = SkeletonNodeTypeV2::Endpoint;
    std::vector<SkeletonPoint> members;
    std::vector<int> portIds;
    double centerGridX = 0.0;
    double centerGridY = 0.0;
    double centerWorldX = 0.0;
    double centerWorldY = 0.0;
};

struct SkeletonPortV2 {
    int id = 0;
    int nodeId = 0;
    int branchId = 0;
    int halfEdgeId = 0;
    SkeletonPoint anchor;
    SkeletonPoint exitPoint;
    double directionX = 0.0;
    double directionY = 0.0;
};

struct SkeletonHalfEdgeV2 {
    int id = 0;
    int branchId = 0;
    int originNodeId = 0;
    int targetNodeId = 0;
    int portId = 0;
    int twinId = 0;
    double physicalLength = 0.0;
};

struct SkeletonBranchV2 {
    int id = 0;
    int startNodeId = 0;
    int endNodeId = 0;
    int startPortId = 0;
    int endPortId = 0;
    int forwardHalfEdgeId = 0;
    int reverseHalfEdgeId = 0;
    bool closedLoop = false;
    double physicalLength = 0.0;
    std::vector<SkeletonPoint> points;
};

struct SkeletonGraphV2Stats {
    int skeletonPixels = 0;
    int endpointPixels = 0;
    int rawJunctionPixels = 0;
    int junctionClusters = 0;
    int isolatedPixels = 0;
    int loopNodes = 0;
    int branches = 0;
    int ports = 0;
    int halfEdges = 0;
    int ambiguousPortAttachments = 0;
    int syntheticEndpointNodes = 0;
};

struct SkeletonGraphV2 {
    std::vector<SkeletonNodeV2> nodes;
    std::vector<SkeletonPortV2> ports;
    std::vector<SkeletonHalfEdgeV2> halfEdges;
    std::vector<SkeletonBranchV2> branches;
    SkeletonGraphV2Stats stats;
};

SkeletonGraphV2 buildSkeletonGraphV2(
    const BinaryMask& skeleton,
    const GridMetric& metric);

} // namespace fpt
