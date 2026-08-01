#pragma once

#include "Polygon.h"

#include <string>
#include <vector>

namespace fpt {

struct SegmentIntersectionDetail {
    std::string type;
    int polygonAId = 0;
    int polygonBId = 0;
    int edgeA = 0;
    int edgeB = 0;
    Point point;
};

struct TopologyDiagnostics {
    int openRingCount = 0;
    int selfIntersectingPolygonCount = 0;
    int polygonIntersectionCount = 0;
    int invalidPolygonCount = 0;
    int nonFiniteCoordinateCount = 0;
    int zeroLengthEdgeCount = 0;
    int duplicateVertexRingCount = 0;
    int degenerateRingCount = 0;
    int holeOutsideShellCount = 0;
    int nestedHoleCount = 0;
    int polygonContainmentCount = 0;
    int polygonInteriorOverlapCount = 0;
    std::vector<int> openRingPolygonIds;
    std::vector<int> selfIntersectingPolygonIds;
    std::vector<int> invalidPolygonIds;
    std::vector<SegmentIntersectionDetail> selfIntersectionDetails;
    std::vector<SegmentIntersectionDetail> polygonIntersectionDetails;
    std::vector<SegmentIntersectionDetail> geometryValidityDetails;
};

TopologyDiagnostics diagnoseTopology(const std::vector<Polygon>& polygons);
TopologyDiagnostics diagnoseTopology(const std::vector<Polygon>& polygons, bool keptOnly);
bool topologyIsValid(const TopologyDiagnostics& diagnostics);
bool ringBoundariesConflict(
    const std::vector<Point>& first,
    const std::vector<Point>& second,
    bool rejectMultiplePointTouches = true);
bool polygonHasTopologyConflict(
    const Polygon& candidate,
    const std::vector<Polygon>& polygons,
    bool keptOnly);

} // namespace fpt
