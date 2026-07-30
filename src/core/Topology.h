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
    std::vector<int> openRingPolygonIds;
    std::vector<int> selfIntersectingPolygonIds;
    std::vector<SegmentIntersectionDetail> selfIntersectionDetails;
    std::vector<SegmentIntersectionDetail> polygonIntersectionDetails;
};

TopologyDiagnostics diagnoseTopology(const std::vector<Polygon>& polygons);
TopologyDiagnostics diagnoseTopology(const std::vector<Polygon>& polygons, bool keptOnly);
bool ringBoundariesConflict(
    const std::vector<Point>& first,
    const std::vector<Point>& second,
    bool rejectMultiplePointTouches = true);
bool polygonHasTopologyConflict(
    const Polygon& candidate,
    const std::vector<Polygon>& polygons,
    bool keptOnly);

} // namespace fpt
