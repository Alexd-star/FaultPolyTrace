#include "Topology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

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

enum class PointLocation {
    outside,
    inside,
    boundary
};

struct PolygonConflictResult {
    bool boundaryConflict = false;
    bool properBoundaryCrossing = false;
    bool containment = false;
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

bool segmentsProperlyCross(const Point& a, const Point& b, const Point& c, const Point& d) {
    const double o1 = orient(a, b, c);
    const double o2 = orient(a, b, d);
    const double o3 = orient(c, d, a);
    const double o4 = orient(c, d, b);
    return ((o1 > Epsilon && o2 < -Epsilon) || (o1 < -Epsilon && o2 > Epsilon)) &&
        ((o3 > Epsilon && o4 < -Epsilon) || (o3 < -Epsilon && o4 > Epsilon));
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

void addValidityDetail(
    TopologyDiagnostics& diagnostics,
    const std::string& type,
    int polygonId,
    int ringIndex,
    int vertexIndex,
    const Point& point) {
    diagnostics.geometryValidityDetails.push_back({
        type, polygonId, polygonId, ringIndex, vertexIndex, point
    });
}

bool validateRingBasics(
    const std::vector<Point>& ring,
    const Polygon& polygon,
    int ringIndex,
    TopologyDiagnostics& diagnostics) {
    bool invalid = false;
    if (ring.size() < 3) {
        ++diagnostics.openRingCount;
        diagnostics.openRingPolygonIds.push_back(polygon.id);
        addValidityDetail(diagnostics, "ring-too-short", polygon.id, ringIndex,
                          static_cast<int>(ring.size()), {});
        return true;
    }

    bool hasNonFinite = false;
    std::vector<std::tuple<double, double, std::size_t>> orderedVertices;
    orderedVertices.reserve(ring.size());
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Point& point = ring[i];
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            hasNonFinite = true;
            invalid = true;
            ++diagnostics.nonFiniteCoordinateCount;
            addValidityDetail(diagnostics, "non-finite-coordinate", polygon.id,
                              ringIndex, static_cast<int>(i), point);
            continue;
        }
        orderedVertices.emplace_back(point.x, point.y, i);
    }

    bool duplicateVertex = false;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const std::size_t next = (i + 1) % ring.size();
        if (std::isfinite(ring[i].x) && std::isfinite(ring[i].y) &&
            std::isfinite(ring[next].x) && std::isfinite(ring[next].y) &&
            samePoint(ring[i], ring[next])) {
            invalid = true;
            duplicateVertex = true;
            ++diagnostics.zeroLengthEdgeCount;
            addValidityDetail(diagnostics, "zero-length-edge", polygon.id,
                              ringIndex, static_cast<int>(i), ring[i]);
        }
    }

    std::sort(orderedVertices.begin(), orderedVertices.end());
    for (std::size_t i = 1; i < orderedVertices.size(); ++i) {
        const Point previous{std::get<0>(orderedVertices[i - 1]),
                             std::get<1>(orderedVertices[i - 1])};
        const Point current{std::get<0>(orderedVertices[i]),
                            std::get<1>(orderedVertices[i])};
        if (samePoint(previous, current)) {
            duplicateVertex = true;
            invalid = true;
            addValidityDetail(
                diagnostics, "duplicate-ring-vertex", polygon.id, ringIndex,
                static_cast<int>(std::get<2>(orderedVertices[i])), current);
            break;
        }
    }
    if (duplicateVertex) {
        ++diagnostics.duplicateVertexRingCount;
    }

    if (!hasNonFinite) {
        double minX = ring.front().x;
        double maxX = ring.front().x;
        double minY = ring.front().y;
        double maxY = ring.front().y;
        for (const Point& point : ring) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
        const double scale = std::max({1.0, maxX - minX, maxY - minY});
        const double areaTolerance = 1e-12 * scale * scale;
        if (polygonArea(ring) <= areaTolerance) {
            invalid = true;
            ++diagnostics.degenerateRingCount;
            addValidityDetail(diagnostics, "zero-area-ring", polygon.id,
                              ringIndex, -1, ring.front());
        }
    }
    return invalid;
}

PointLocation locatePointInRing(const Point& point, const std::vector<Point>& ring) {
    if (ring.size() < 3 || !std::isfinite(point.x) || !std::isfinite(point.y)) {
        return PointLocation::outside;
    }
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Point& a = ring[j];
        const Point& b = ring[i];
        if (!std::isfinite(a.x) || !std::isfinite(a.y) ||
            !std::isfinite(b.x) || !std::isfinite(b.y)) {
            return PointLocation::outside;
        }
        if (onSegment(a, b, point)) {
            return PointLocation::boundary;
        }
        const bool crossesScanline = (a.y > point.y) != (b.y > point.y);
        if (crossesScanline) {
            const double intersectionX =
                a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
            if (intersectionX > point.x) {
                inside = !inside;
            }
        }
    }
    return inside ? PointLocation::inside : PointLocation::outside;
}

PointLocation locatePointInPolygonArea(const Point& point, const Polygon& polygon) {
    const PointLocation shell = locatePointInRing(point, polygon.points);
    if (shell != PointLocation::inside) {
        return shell;
    }
    for (const std::vector<Point>& hole : polygon.holes) {
        const PointLocation location = locatePointInRing(point, hole);
        if (location == PointLocation::boundary) {
            return PointLocation::boundary;
        }
        if (location == PointLocation::inside) {
            return PointLocation::outside;
        }
    }
    return PointLocation::inside;
}

bool ringHasPointInside(const std::vector<Point>& candidate, const std::vector<Point>& container) {
    return std::any_of(candidate.begin(), candidate.end(), [&](const Point& point) {
        return locatePointInRing(point, container) == PointLocation::inside;
    });
}

bool polygonHasPointInside(const Polygon& candidate, const Polygon& container) {
    return std::any_of(candidate.points.begin(), candidate.points.end(), [&](const Point& point) {
        return locatePointInPolygonArea(point, container) == PointLocation::inside;
    });
}

bool validateHoleRelationships(const Polygon& polygon, TopologyDiagnostics& diagnostics) {
    bool invalid = false;
    for (std::size_t i = 0; i < polygon.holes.size(); ++i) {
        const std::vector<Point>& hole = polygon.holes[i];
        bool hasInteriorPoint = false;
        bool hasExteriorPoint = false;
        for (const Point& point : hole) {
            const PointLocation location = locatePointInRing(point, polygon.points);
            hasInteriorPoint = hasInteriorPoint || location == PointLocation::inside;
            hasExteriorPoint = hasExteriorPoint || location == PointLocation::outside;
        }
        if (hasExteriorPoint || !hasInteriorPoint) {
            invalid = true;
            ++diagnostics.holeOutsideShellCount;
            addValidityDetail(diagnostics, "hole-outside-shell", polygon.id,
                              static_cast<int>(i + 1), -1,
                              hole.empty() ? Point{} : hole.front());
        }

        for (std::size_t j = i + 1; j < polygon.holes.size(); ++j) {
            if (ringHasPointInside(hole, polygon.holes[j]) ||
                ringHasPointInside(polygon.holes[j], hole)) {
                invalid = true;
                ++diagnostics.nestedHoleCount;
                addValidityDetail(diagnostics, "nested-holes", polygon.id,
                                  static_cast<int>(i + 1), static_cast<int>(j + 1),
                                  hole.empty() ? Point{} : hole.front());
            }
        }
    }
    return invalid;
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

PolygonConflictResult polygonConflict(
    const Polygon& aPoly,
    const Polygon& bPoly,
    TopologyDiagnostics& diagnostics) {
    PolygonConflictResult result;
    if (aPoly.points.empty() || bPoly.points.empty()) {
        return result;
    }
    if (!boundsOverlap(polygonBounds(aPoly), polygonBounds(bPoly))) {
        return result;
    }
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
                result.boundaryConflict = true;
                result.properBoundaryCrossing = result.properBoundaryCrossing ||
                    segmentsProperlyCross(*a.a, *a.b, *b.a, *b.b);
                diagnostics.polygonIntersectionDetails.push_back({
                    "polygon-boundary-conflict",
                    aPoly.id,
                    bPoly.id,
                    static_cast<int>(i),
                    static_cast<int>(j),
                    approximateIntersectionPoint(*a.a, *a.b, *b.a, *b.b)
                });
            }
        }
    }
    if (!result.boundaryConflict &&
        (polygonHasPointInside(aPoly, bPoly) || polygonHasPointInside(bPoly, aPoly))) {
        result.containment = true;
        const Point representative = polygonHasPointInside(aPoly, bPoly)
            ? aPoly.points.front() : bPoly.points.front();
        diagnostics.polygonIntersectionDetails.push_back({
            "polygon-containment",
            aPoly.id,
            bPoly.id,
            -1,
            -1,
            representative
        });
    }
    return result;
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
    std::vector<bool> validPolygons(polygons.size(), false);
    for (std::size_t polygonIndex = 0; polygonIndex < polygons.size(); ++polygonIndex) {
        const Polygon& polygon = polygons[polygonIndex];
        if (keptOnly && !polygon.kept) {
            continue;
        }

        bool invalid = validateRingBasics(polygon.points, polygon, 0, diagnostics);
        for (std::size_t i = 0; i < polygon.holes.size(); ++i) {
            invalid = validateRingBasics(
                polygon.holes[i], polygon, static_cast<int>(i + 1), diagnostics) || invalid;
        }

        bool selfIntersects = false;
        if (!invalid) {
            selfIntersects = polygonSelfIntersects(polygon, diagnostics);
            invalid = validateHoleRelationships(polygon, diagnostics) || invalid;
        }
        if (selfIntersects) {
            ++diagnostics.selfIntersectingPolygonCount;
            diagnostics.selfIntersectingPolygonIds.push_back(polygon.id);
            invalid = true;
        }
        if (invalid) {
            ++diagnostics.invalidPolygonCount;
            diagnostics.invalidPolygonIds.push_back(polygon.id);
        } else {
            validPolygons[polygonIndex] = true;
        }
    }

    for (std::size_t i = 0; i < polygons.size(); ++i) {
        if ((keptOnly && !polygons[i].kept) || !validPolygons[i]) {
            continue;
        }
        for (std::size_t j = i + 1; j < polygons.size(); ++j) {
            if ((keptOnly && !polygons[j].kept) || !validPolygons[j]) {
                continue;
            }
            const PolygonConflictResult conflict =
                polygonConflict(polygons[i], polygons[j], diagnostics);
            if (conflict.boundaryConflict || conflict.containment) {
                ++diagnostics.polygonIntersectionCount;
            }
            if (conflict.containment) {
                ++diagnostics.polygonContainmentCount;
            }
            if (conflict.properBoundaryCrossing || conflict.containment) {
                ++diagnostics.polygonInteriorOverlapCount;
            }
        }
    }
    return diagnostics;
}

bool topologyIsValid(const TopologyDiagnostics& diagnostics) {
    return diagnostics.invalidPolygonCount == 0 &&
        diagnostics.openRingCount == 0 &&
        diagnostics.selfIntersectingPolygonCount == 0 &&
        diagnostics.polygonIntersectionCount == 0;
}

bool polygonHasTopologyConflict(
    const Polygon& candidate,
    const std::vector<Polygon>& polygons,
    bool keptOnly) {
    const TopologyDiagnostics candidateDiagnostics = diagnoseTopology({candidate}, false);
    if (!topologyIsValid(candidateDiagnostics)) {
        return true;
    }

    TopologyDiagnostics diagnostics;
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
        const PolygonConflictResult conflict = polygonConflict(candidate, polygon, diagnostics);
        if (conflict.boundaryConflict || conflict.containment) {
            return true;
        }
    }
    return false;
}

} // namespace fpt
