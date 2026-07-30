#include "Topology.h"

#include <algorithm>
#include <cmath>

namespace fpt {
namespace {

constexpr double Epsilon = 1e-9;

struct Bounds {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

struct SegmentRef {
    const Point* a = nullptr;
    const Point* b = nullptr;
    Bounds bounds;
};

Bounds makeBounds(const Point& a, const Point& b) {
    return {
        std::min(a.x, b.x),
        std::min(a.y, b.y),
        std::max(a.x, b.x),
        std::max(a.y, b.y)
    };
}

Bounds polygonBounds(const Polygon& polygon) {
    Bounds bounds{polygon.points.front().x, polygon.points.front().y, polygon.points.front().x, polygon.points.front().y};
    for (const Point& point : polygon.points) {
        bounds.minX = std::min(bounds.minX, point.x);
        bounds.minY = std::min(bounds.minY, point.y);
        bounds.maxX = std::max(bounds.maxX, point.x);
        bounds.maxY = std::max(bounds.maxY, point.y);
    }
    for (const std::vector<Point>& hole : polygon.holes) {
        for (const Point& point : hole) {
            bounds.minX = std::min(bounds.minX, point.x);
            bounds.minY = std::min(bounds.minY, point.y);
            bounds.maxX = std::max(bounds.maxX, point.x);
            bounds.maxY = std::max(bounds.maxY, point.y);
        }
    }
    return bounds;
}

bool boundsOverlap(const Bounds& a, const Bounds& b) {
    return a.minX <= b.maxX + Epsilon
        && a.maxX + Epsilon >= b.minX
        && a.minY <= b.maxY + Epsilon
        && a.maxY + Epsilon >= b.minY;
}

std::vector<SegmentRef> buildSegmentsFromRing(const std::vector<Point>& ring) {
    std::vector<SegmentRef> segments;
    segments.reserve(ring.size());
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Point& a = ring[i];
        const Point& b = ring[(i + 1) % ring.size()];
        segments.push_back({&a, &b, makeBounds(a, b)});
    }
    return segments;
}

std::vector<SegmentRef> buildSegments(const Polygon& polygon) {
    std::vector<SegmentRef> segments = buildSegmentsFromRing(polygon.points);
    for (const std::vector<Point>& hole : polygon.holes) {
        if (hole.size() < 3) {
            continue;
        }
        std::vector<SegmentRef> holeSegments = buildSegmentsFromRing(hole);
        segments.insert(segments.end(), holeSegments.begin(), holeSegments.end());
    }
    return segments;
}

double orient(const Point& a, const Point& b, const Point& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

bool onSegment(const Point& a, const Point& b, const Point& p) {
    return std::abs(orient(a, b, p)) <= Epsilon
        && p.x >= std::min(a.x, b.x) - Epsilon
        && p.x <= std::max(a.x, b.x) + Epsilon
        && p.y >= std::min(a.y, b.y) - Epsilon
        && p.y <= std::max(a.y, b.y) + Epsilon;
}

bool collinearSegmentsOverlapInLine(const Point& a, const Point& b, const Point& c, const Point& d) {
    if (std::abs(orient(a, b, c)) > Epsilon || std::abs(orient(a, b, d)) > Epsilon) {
        return false;
    }
    const double overlapX = std::min(std::max(a.x, b.x), std::max(c.x, d.x)) -
        std::max(std::min(a.x, b.x), std::min(c.x, d.x));
    const double overlapY = std::min(std::max(a.y, b.y), std::max(c.y, d.y)) -
        std::max(std::min(a.y, b.y), std::min(c.y, d.y));
    return std::max(overlapX, overlapY) > Epsilon;
}

bool segmentsCrossOrOverlap(const Point& a, const Point& b, const Point& c, const Point& d) {
    const double o1 = orient(a, b, c);
    const double o2 = orient(a, b, d);
    const double o3 = orient(c, d, a);
    const double o4 = orient(c, d, b);

    const bool straddlesAB = (o1 > Epsilon && o2 < -Epsilon) || (o1 < -Epsilon && o2 > Epsilon);
    const bool straddlesCD = (o3 > Epsilon && o4 < -Epsilon) || (o3 < -Epsilon && o4 > Epsilon);
    if (straddlesAB && straddlesCD) {
        return true;
    }
    return collinearSegmentsOverlapInLine(a, b, c, d);
}

bool segmentsIntersect(const Point& a, const Point& b, const Point& c, const Point& d) {
    const double o1 = orient(a, b, c);
    const double o2 = orient(a, b, d);
    const double o3 = orient(c, d, a);
    const double o4 = orient(c, d, b);

    const bool straddlesAB = (o1 > Epsilon && o2 < -Epsilon) || (o1 < -Epsilon && o2 > Epsilon);
    const bool straddlesCD = (o3 > Epsilon && o4 < -Epsilon) || (o3 < -Epsilon && o4 > Epsilon);
    if (straddlesAB && straddlesCD) {
        return true;
    }
    return onSegment(a, b, c) || onSegment(a, b, d) || onSegment(c, d, a) || onSegment(c, d, b);
}

Point approximateIntersectionPoint(const Point& a, const Point& b, const Point& c, const Point& d) {
    const double rX = b.x - a.x;
    const double rY = b.y - a.y;
    const double sX = d.x - c.x;
    const double sY = d.y - c.y;
    const double denominator = rX * sY - rY * sX;
    if (std::abs(denominator) > Epsilon) {
        const double t = ((c.x - a.x) * sY - (c.y - a.y) * sX) / denominator;
        return {a.x + t * rX, a.y + t * rY};
    }

    std::vector<Point> candidates;
    if (onSegment(a, b, c)) {
        candidates.push_back(c);
    }
    if (onSegment(a, b, d)) {
        candidates.push_back(d);
    }
    if (onSegment(c, d, a)) {
        candidates.push_back(a);
    }
    if (onSegment(c, d, b)) {
        candidates.push_back(b);
    }
    if (candidates.empty()) {
        return {(a.x + b.x + c.x + d.x) * 0.25, (a.y + b.y + c.y + d.y) * 0.25};
    }

    Point point;
    for (const Point& candidate : candidates) {
        point.x += candidate.x;
        point.y += candidate.y;
    }
    point.x /= static_cast<double>(candidates.size());
    point.y /= static_cast<double>(candidates.size());
    return point;
}

bool adjacentEdges(std::size_t edgeA, std::size_t edgeB, std::size_t edgeCount) {
    if (edgeA == edgeB) {
        return true;
    }
    if ((edgeA + 1) % edgeCount == edgeB) {
        return true;
    }
    if ((edgeB + 1) % edgeCount == edgeA) {
        return true;
    }
    return false;
}

bool samePoint(const Point& first, const Point& second) {
    return std::abs(first.x - second.x) <= Epsilon &&
        std::abs(first.y - second.y) <= Epsilon;
}

bool ringBoundaryConflict(
    const std::vector<Point>& first,
    const std::vector<Point>& second,
    bool rejectMultiplePointTouches) {
    if (first.size() < 3 || second.size() < 3) {
        return false;
    }
    const std::vector<SegmentRef> firstSegments = buildSegmentsFromRing(first);
    const std::vector<SegmentRef> secondSegments = buildSegmentsFromRing(second);
    std::vector<Point> pointTouches;
    for (const SegmentRef& a : firstSegments) {
        for (const SegmentRef& b : secondSegments) {
            if (!boundsOverlap(a.bounds, b.bounds)) {
                continue;
            }
            if (segmentsCrossOrOverlap(*a.a, *a.b, *b.a, *b.b)) {
                return true;
            }
            if (!rejectMultiplePointTouches ||
                !segmentsIntersect(*a.a, *a.b, *b.a, *b.b)) {
                continue;
            }
            const Point contact = approximateIntersectionPoint(*a.a, *a.b, *b.a, *b.b);
            const bool duplicate = std::any_of(
                pointTouches.begin(),
                pointTouches.end(),
                [&](const Point& existing) { return samePoint(existing, contact); });
            if (!duplicate) {
                pointTouches.push_back(contact);
                if (pointTouches.size() > 1) {
                    return true;
                }
            }
        }
    }
    return false;
}

bool ringSelfIntersects(const std::vector<Point>& ring, const Polygon& polygon, int ringIndex, TopologyDiagnostics& diagnostics) {
    const std::size_t n = ring.size();
    if (n < 4) {
        return false;
    }
    bool found = false;
    const std::vector<SegmentRef> segments = buildSegmentsFromRing(ring);
    for (std::size_t i = 0; i < n; ++i) {
        const SegmentRef& first = segments[i];
        for (std::size_t j = i + 1; j < n; ++j) {
            if (adjacentEdges(i, j, n)) {
                continue;
            }
            const SegmentRef& second = segments[j];
            if (!boundsOverlap(first.bounds, second.bounds)) {
                continue;
            }
            if (segmentsIntersect(*first.a, *first.b, *second.a, *second.b)) {
                found = true;
                diagnostics.selfIntersectionDetails.push_back({
                    ringIndex == 0 ? "self" : "hole-self",
                    polygon.id,
                    polygon.id,
                    static_cast<int>(i),
                    static_cast<int>(j),
                    approximateIntersectionPoint(*first.a, *first.b, *second.a, *second.b)
                });
            }
        }
    }
    return found;
}

bool ringsIntersect(
    const std::vector<Point>& aRing,
    const std::vector<Point>& bRing,
    const Polygon& polygon,
    int ringAIndex,
    int ringBIndex,
    TopologyDiagnostics& diagnostics) {
    if (aRing.size() < 3 || bRing.size() < 3) {
        return false;
    }

    if (!ringBoundaryConflict(aRing, bRing, true)) {
        return false;
    }
    const std::vector<SegmentRef> aSegments = buildSegmentsFromRing(aRing);
    const std::vector<SegmentRef> bSegments = buildSegmentsFromRing(bRing);
    for (std::size_t i = 0; i < aSegments.size(); ++i) {
        const SegmentRef& a = aSegments[i];
        for (std::size_t j = 0; j < bSegments.size(); ++j) {
            const SegmentRef& b = bSegments[j];
            if (!boundsOverlap(a.bounds, b.bounds)) {
                continue;
            }
            if (segmentsIntersect(*a.a, *a.b, *b.a, *b.b)) {
                diagnostics.selfIntersectionDetails.push_back({
                    ringAIndex == 0 || ringBIndex == 0 ? "hole-exterior" : "hole-hole",
                    polygon.id,
                    polygon.id,
                    static_cast<int>(i),
                    static_cast<int>(j),
                    approximateIntersectionPoint(*a.a, *a.b, *b.a, *b.b)
                });
                return true;
            }
        }
    }
    return true;
}

bool polygonSelfIntersects(const Polygon& polygon, TopologyDiagnostics& diagnostics) {
    bool found = ringSelfIntersects(polygon.points, polygon, 0, diagnostics);
    for (std::size_t i = 0; i < polygon.holes.size(); ++i) {
        found = ringSelfIntersects(polygon.holes[i], polygon, static_cast<int>(i + 1), diagnostics) || found;
        found = ringsIntersect(polygon.points, polygon.holes[i], polygon, 0, static_cast<int>(i + 1), diagnostics) || found;
        for (std::size_t j = i + 1; j < polygon.holes.size(); ++j) {
            found = ringsIntersect(polygon.holes[i], polygon.holes[j], polygon, static_cast<int>(i + 1), static_cast<int>(j + 1), diagnostics) || found;
        }
    }
    return found;
}

bool polygonsIntersect(const Polygon& aPoly, const Polygon& bPoly, TopologyDiagnostics& diagnostics) {
    if (aPoly.points.empty() || bPoly.points.empty()) {
        return false;
    }
    if (!boundsOverlap(polygonBounds(aPoly), polygonBounds(bPoly))) {
        return false;
    }
    bool found = false;
    const std::vector<SegmentRef> aSegments = buildSegments(aPoly);
    const std::vector<SegmentRef> bSegments = buildSegments(bPoly);
    for (std::size_t i = 0; i < aSegments.size(); ++i) {
        const SegmentRef& a = aSegments[i];
        for (std::size_t j = 0; j < bSegments.size(); ++j) {
            const SegmentRef& b = bSegments[j];
            if (!boundsOverlap(a.bounds, b.bounds)) {
                continue;
            }
            if (segmentsCrossOrOverlap(*a.a, *a.b, *b.a, *b.b)) {
                found = true;
                diagnostics.polygonIntersectionDetails.push_back({
                    "polygon",
                    aPoly.id,
                    bPoly.id,
                    static_cast<int>(i),
                    static_cast<int>(j),
                    approximateIntersectionPoint(*a.a, *a.b, *b.a, *b.b)
                });
            }
        }
    }
    return found;
}

} // namespace

bool ringBoundariesConflict(
    const std::vector<Point>& first,
    const std::vector<Point>& second,
    bool rejectMultiplePointTouches) {
    return ringBoundaryConflict(first, second, rejectMultiplePointTouches);
}

TopologyDiagnostics diagnoseTopology(const std::vector<Polygon>& polygons) {
    return diagnoseTopology(polygons, false);
}

TopologyDiagnostics diagnoseTopology(const std::vector<Polygon>& polygons, bool keptOnly) {
    TopologyDiagnostics diagnostics;
    for (const Polygon& polygon : polygons) {
        if (keptOnly && !polygon.kept) {
            continue;
        }
        if (polygon.points.size() < 3) {
            ++diagnostics.openRingCount;
            diagnostics.openRingPolygonIds.push_back(polygon.id);
            continue;
        }
        // Internal polygon representation omits the duplicated final vertex. It is considered closed
        // when it has at least three vertices and is consumed with modulo edge indexing.
        if (polygonSelfIntersects(polygon, diagnostics)) {
            ++diagnostics.selfIntersectingPolygonCount;
            diagnostics.selfIntersectingPolygonIds.push_back(polygon.id);
        }
    }

    for (std::size_t i = 0; i < polygons.size(); ++i) {
        if (keptOnly && !polygons[i].kept) {
            continue;
        }
        for (std::size_t j = i + 1; j < polygons.size(); ++j) {
            if (keptOnly && !polygons[j].kept) {
                continue;
            }
            if (polygonsIntersect(polygons[i], polygons[j], diagnostics)) {
                ++diagnostics.polygonIntersectionCount;
            }
        }
    }
    return diagnostics;
}

bool polygonHasTopologyConflict(
    const Polygon& candidate,
    const std::vector<Polygon>& polygons,
    bool keptOnly) {
    if (candidate.points.size() < 3) {
        return true;
    }

    TopologyDiagnostics diagnostics;
    if (polygonSelfIntersects(candidate, diagnostics)) {
        return true;
    }

    for (const Polygon& polygon : polygons) {
        if (polygon.id == candidate.id) {
            continue;
        }
        if (keptOnly && !polygon.kept) {
            continue;
        }
        if (polygon.points.size() < 3) {
            continue;
        }
        if (polygonsIntersect(candidate, polygon, diagnostics)) {
            return true;
        }
    }
    return false;
}

} // namespace fpt
