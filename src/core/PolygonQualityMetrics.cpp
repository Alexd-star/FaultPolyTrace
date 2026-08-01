#include "PolygonQualityMetrics.h"

#include "Topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fpt {
namespace {

constexpr double kRasterEpsilon = 1e-9;

bool pointInRing(const Point& point, const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return false;
    }
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1;
         i < ring.size(); j = i++) {
        const Point& first = ring[i];
        const Point& second = ring[j];
        if (((first.y > point.y) != (second.y > point.y)) &&
            point.x < (second.x - first.x) *
                    (point.y - first.y) /
                    ((second.y - first.y) + 1e-30) + first.x) {
            inside = !inside;
        }
    }
    return inside;
}

double ringCurvatureEnergy(const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return 0.0;
    }
    double energy = 0.0;
    double perimeter = 0.0;
    for (std::size_t index = 0; index < ring.size(); ++index) {
        const Point& previous =
            ring[(index + ring.size() - 1) % ring.size()];
        const Point& current = ring[index];
        const Point& next = ring[(index + 1) % ring.size()];
        const double incomingX = current.x - previous.x;
        const double incomingY = current.y - previous.y;
        const double outgoingX = next.x - current.x;
        const double outgoingY = next.y - current.y;
        const double incomingLength = std::hypot(incomingX, incomingY);
        const double outgoingLength = std::hypot(outgoingX, outgoingY);
        perimeter += outgoingLength;
        if (incomingLength <= 1e-12 || outgoingLength <= 1e-12) {
            continue;
        }
        const double cosine = std::clamp(
            (incomingX * outgoingX + incomingY * outgoingY) /
                (incomingLength * outgoingLength),
            -1.0,
            1.0);
        const double turn = std::acos(cosine);
        const double localArc = 0.5 * (incomingLength + outgoingLength);
        energy += turn * turn / std::max(localArc, 1e-12);
    }
    return perimeter > 1e-12 ? energy / perimeter : 0.0;
}

double pointSegmentDistanceSquared(
    const Point& point,
    const Point& first,
    const Point& second) {
    const double dx = second.x - first.x;
    const double dy = second.y - first.y;
    const double lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1e-18) {
        const double px = point.x - first.x;
        const double py = point.y - first.y;
        return px * px + py * py;
    }
    const double projection = std::clamp(
        ((point.x - first.x) * dx +
         (point.y - first.y) * dy) / lengthSquared,
        0.0,
        1.0);
    const double px = point.x - (first.x + projection * dx);
    const double py = point.y - (first.y + projection * dy);
    return px * px + py * py;
}

double pointToRingDistance(
    const Point& point,
    const std::vector<Point>& ring) {
    if (pointInRing(point, ring)) {
        return 0.0;
    }
    if (ring.size() < 2) {
        return std::numeric_limits<double>::infinity();
    }
    double best = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < ring.size(); ++index) {
        best = std::min(
            best,
            pointSegmentDistanceSquared(
                point,
                ring[index],
                ring[(index + 1) % ring.size()]));
    }
    return std::sqrt(best);
}

std::vector<double> scanlineIntersections(
    const std::vector<Point>& ring,
    double y) {
    std::vector<double> intersections;
    intersections.reserve(ring.size());
    if (ring.size() < 3) {
        return intersections;
    }
    for (std::size_t i = 0, j = ring.size() - 1;
         i < ring.size(); j = i++) {
        const Point& first = ring[i];
        const Point& second = ring[j];
        if ((first.y > y) != (second.y > y)) {
            intersections.push_back(
                (second.x - first.x) *
                    (y - first.y) /
                    ((second.y - first.y) + 1e-30) +
                first.x);
        }
    }
    std::sort(intersections.begin(), intersections.end());
    return intersections;
}

bool pointInsideFromIntersections(
    double x,
    const std::vector<double>& intersections) {
    const auto firstToRight = std::upper_bound(
        intersections.begin(), intersections.end(), x);
    return (std::distance(firstToRight, intersections.end()) & 1) != 0;
}

struct PolygonScanline {
    std::vector<double> exterior;
    std::vector<std::vector<double>> holes;
};

PolygonScanline buildPolygonScanline(
    const Polygon& polygon,
    double y) {
    PolygonScanline scanline;
    scanline.exterior = scanlineIntersections(polygon.points, y);
    scanline.holes.reserve(polygon.holes.size());
    for (const std::vector<Point>& hole : polygon.holes) {
        scanline.holes.push_back(scanlineIntersections(hole, y));
    }
    return scanline;
}

bool pointInsidePolygonScanline(
    double x,
    const PolygonScanline& scanline) {
    if (!pointInsideFromIntersections(x, scanline.exterior)) {
        return false;
    }
    for (const std::vector<double>& hole : scanline.holes) {
        if (pointInsideFromIntersections(x, hole)) {
            return false;
        }
    }
    return true;
}

double orient(const Point& first, const Point& second, const Point& point) {
    return (second.x - first.x) * (point.y - first.y) -
        (second.y - first.y) * (point.x - first.x);
}

bool collinearSegmentsOverlap(
    const Point& firstA,
    const Point& firstB,
    const Point& secondA,
    const Point& secondB) {
    if (std::abs(orient(firstA, firstB, secondA)) > kRasterEpsilon ||
        std::abs(orient(firstA, firstB, secondB)) > kRasterEpsilon) {
        return false;
    }
    const double overlapX =
        std::min(std::max(firstA.x, firstB.x),
                 std::max(secondA.x, secondB.x)) -
        std::max(std::min(firstA.x, firstB.x),
                 std::min(secondA.x, secondB.x));
    const double overlapY =
        std::min(std::max(firstA.y, firstB.y),
                 std::max(secondA.y, secondB.y)) -
        std::max(std::min(firstA.y, firstB.y),
                 std::min(secondA.y, secondB.y));
    return std::max(overlapX, overlapY) > kRasterEpsilon;
}

bool segmentsCrossOrOverlap(
    const Point& firstA,
    const Point& firstB,
    const Point& secondA,
    const Point& secondB) {
    const double o1 = orient(firstA, firstB, secondA);
    const double o2 = orient(firstA, firstB, secondB);
    const double o3 = orient(secondA, secondB, firstA);
    const double o4 = orient(secondA, secondB, firstB);
    const bool firstStraddles =
        (o1 > kRasterEpsilon && o2 < -kRasterEpsilon) ||
        (o1 < -kRasterEpsilon && o2 > kRasterEpsilon);
    const bool secondStraddles =
        (o3 > kRasterEpsilon && o4 < -kRasterEpsilon) ||
        (o3 < -kRasterEpsilon && o4 > kRasterEpsilon);
    return (firstStraddles && secondStraddles) ||
        collinearSegmentsOverlap(firstA, firstB, secondA, secondB);
}

bool valueInClosedInterval(double value, double first, double second) {
    return value >= std::min(first, second) - kRasterEpsilon &&
        value <= std::max(first, second) + kRasterEpsilon;
}

bool segmentIntersectsCellArea(
    const Point& first,
    const Point& second,
    double minX,
    double minY,
    double maxX,
    double maxY) {
    if (std::max(first.x, second.x) + kRasterEpsilon < minX ||
        std::min(first.x, second.x) > maxX + kRasterEpsilon ||
        std::max(first.y, second.y) + kRasterEpsilon < minY ||
        std::min(first.y, second.y) > maxY + kRasterEpsilon) {
        return false;
    }
    const std::array<Point, 4> corners{{
        {minX, minY}, {maxX, minY},
        {maxX, maxY}, {minX, maxY}}};
    for (std::size_t edge = 0; edge < corners.size(); ++edge) {
        if (segmentsCrossOrOverlap(
                corners[edge],
                corners[(edge + 1) % corners.size()],
                first,
                second)) {
            return true;
        }
    }
    return
        (valueInClosedInterval(first.x, minX, maxX) &&
         valueInClosedInterval(first.y, minY, maxY)) ||
        (valueInClosedInterval(second.x, minX, maxX) &&
         valueInClosedInterval(second.y, minY, maxY));
}

} // namespace

bool pointInPolygonArea(
    const Point& point,
    const Polygon& polygon) {
    if (!pointInRing(point, polygon.points)) {
        return false;
    }
    for (const std::vector<Point>& hole : polygon.holes) {
        if (pointInRing(point, hole)) {
            return false;
        }
    }
    return true;
}

bool cellIntersectsPolygonArea(
    const Grid2D& grid,
    int x,
    int y,
    const Polygon& polygon) {
    if (!grid.inBounds(x, y) || polygon.points.size() < 3) {
        return false;
    }
    const double minX = grid.worldX(x);
    const double maxX = grid.worldX(x + 1);
    const double minY = grid.worldY(y);
    const double maxY = grid.worldY(y + 1);
    const std::vector<Point> cellRing{
        {minX, minY}, {maxX, minY},
        {maxX, maxY}, {minX, maxY}};
    const Point center{
        grid.worldX(static_cast<double>(x) + 0.5),
        grid.worldY(static_cast<double>(y) + 0.5)};
    if (pointInPolygonArea(center, polygon)) {
        return true;
    }
    for (const Point& corner : cellRing) {
        if (pointInPolygonArea(corner, polygon)) {
            return true;
        }
    }
    const double halfDiagonal =
        0.5 * std::hypot(grid.dx(), grid.dy());
    if (pointToRingDistance(center, polygon.points) >
        halfDiagonal + 1e-9) {
        return false;
    }
    if (ringBoundariesConflict(cellRing, polygon.points, false)) {
        return true;
    }
    for (const Point& point : polygon.points) {
        if (point.x >= minX && point.x <= maxX &&
            point.y >= minY && point.y <= maxY) {
            return true;
        }
    }
    return false;
}

PolygonCellCoverage rasterizePolygonCellCoverage(
    const Grid2D& grid,
    const Polygon& polygon,
    std::size_t maximumEvaluatedCells) {
    PolygonCellCoverage coverage;
    if (polygon.points.size() < 3 || maximumEvaluatedCells == 0) {
        return coverage;
    }

    double minX = polygon.points.front().x;
    double maxX = minX;
    double minY = polygon.points.front().y;
    double maxY = minY;
    for (const Point& point : polygon.points) {
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
        minY = std::min(minY, point.y);
        maxY = std::max(maxY, point.y);
    }
    coverage.firstX = std::max(0, static_cast<int>(std::floor(
        (minX - grid.sx()) / grid.dx())) - 1);
    const int lastX = std::min(grid.nx() - 1, static_cast<int>(std::ceil(
        (maxX - grid.sx()) / grid.dx())) + 1);
    coverage.firstY = std::max(0, static_cast<int>(std::floor(
        (minY - grid.sy()) / grid.dy())) - 1);
    const int lastY = std::min(grid.ny() - 1, static_cast<int>(std::ceil(
        (maxY - grid.sy()) / grid.dy())) + 1);
    coverage.width = std::max(0, lastX - coverage.firstX + 1);
    coverage.height = std::max(0, lastY - coverage.firstY + 1);
    if (coverage.width == 0 || coverage.height == 0) {
        return coverage;
    }
    coverage.covered.assign(
        static_cast<std::size_t>(coverage.width) *
            static_cast<std::size_t>(coverage.height),
        0);

    std::vector<std::size_t> rowSegments;
    rowSegments.reserve(polygon.points.size());
    for (int y = coverage.firstY; y <= lastY; ++y) {
        const double cellMinY = grid.worldY(y);
        const double cellMaxY = grid.worldY(y + 1);
        const double centerY = grid.worldY(static_cast<double>(y) + 0.5);
        const PolygonScanline bottom =
            buildPolygonScanline(polygon, cellMinY);
        const PolygonScanline center =
            buildPolygonScanline(polygon, centerY);
        const PolygonScanline top =
            buildPolygonScanline(polygon, cellMaxY);

        rowSegments.clear();
        for (std::size_t index = 0; index < polygon.points.size(); ++index) {
            const Point& first = polygon.points[index];
            const Point& second =
                polygon.points[(index + 1) % polygon.points.size()];
            if (std::max(first.y, second.y) + kRasterEpsilon >= cellMinY &&
                std::min(first.y, second.y) <= cellMaxY + kRasterEpsilon) {
                rowSegments.push_back(index);
            }
        }

        for (int x = coverage.firstX; x <= lastX; ++x) {
            if (coverage.evaluatedCells >= maximumEvaluatedCells) {
                coverage.budgetExceeded = true;
                return coverage;
            }
            ++coverage.evaluatedCells;
            const double cellMinX = grid.worldX(x);
            const double cellMaxX = grid.worldX(x + 1);
            const double centerX =
                grid.worldX(static_cast<double>(x) + 0.5);
            bool covered =
                pointInsidePolygonScanline(centerX, center) ||
                pointInsidePolygonScanline(cellMinX, bottom) ||
                pointInsidePolygonScanline(cellMaxX, bottom) ||
                pointInsidePolygonScanline(cellMinX, top) ||
                pointInsidePolygonScanline(cellMaxX, top);
            if (!covered) {
                for (const std::size_t segmentIndex : rowSegments) {
                    const Point& first = polygon.points[segmentIndex];
                    const Point& second = polygon.points[
                        (segmentIndex + 1) % polygon.points.size()];
                    if (segmentIntersectsCellArea(
                            first, second,
                            cellMinX, cellMinY,
                            cellMaxX, cellMaxY)) {
                        covered = true;
                        break;
                    }
                }
            }
            if (covered) {
                const std::size_t localIndex =
                    static_cast<std::size_t>(y - coverage.firstY) *
                        static_cast<std::size_t>(coverage.width) +
                    static_cast<std::size_t>(x - coverage.firstX);
                coverage.covered[localIndex] = 1;
            }
        }
    }
    return coverage;
}

PolygonRasterQuality measurePolygonRasterQuality(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const Polygon& polygon,
    double supportThreshold,
    std::size_t maximumEvaluatedCells) {
    if (supportMask.width() != grid.nx() ||
        supportMask.height() != grid.ny()) {
        throw std::runtime_error(
            "Polygon quality support mask dimensions must match the grid.");
    }
    PolygonRasterQuality result;
    if (polygon.points.size() < 3 || maximumEvaluatedCells == 0) {
        return result;
    }
    const PolygonCellCoverage coverage = rasterizePolygonCellCoverage(
        grid, polygon, maximumEvaluatedCells);
    result.evaluatedCells = coverage.evaluatedCells;
    result.budgetExceeded = coverage.budgetExceeded;
    for (int localY = 0; localY < coverage.height; ++localY) {
        for (int localX = 0; localX < coverage.width; ++localX) {
            const std::size_t localIndex =
                static_cast<std::size_t>(localY) *
                    static_cast<std::size_t>(coverage.width) +
                static_cast<std::size_t>(localX);
            if (coverage.covered[localIndex] == 0) {
                continue;
            }
            const int x = coverage.firstX + localX;
            const int y = coverage.firstY + localY;
            ++result.insideCells;
            const double value = grid.value(x, y);
            if (std::isfinite(value) && supportMask.get(x, y) &&
                value >= supportThreshold) {
                ++result.supportedCells;
            } else {
                ++result.backgroundCells;
            }
        }
    }
    return result;
}

PolygonDualSupportQuality measurePolygonDualSupportQuality(
    const Grid2D& grid,
    const BinaryMask& strongSupportMask,
    const BinaryMask& weakSupportMask,
    const Polygon& polygon,
    double strongSupportThreshold,
    double weakSupportThreshold,
    std::size_t maximumEvaluatedCells,
    double backgroundThreshold) {
    if (strongSupportMask.width() != grid.nx() ||
        strongSupportMask.height() != grid.ny() ||
        weakSupportMask.width() != grid.nx() ||
        weakSupportMask.height() != grid.ny()) {
        throw std::runtime_error(
            "Polygon dual-support mask dimensions must match the grid.");
    }
    if (!std::isfinite(strongSupportThreshold) ||
        !std::isfinite(weakSupportThreshold) ||
        !std::isfinite(backgroundThreshold)) {
        throw std::runtime_error(
            "Polygon dual-support thresholds must be finite.");
    }
    PolygonDualSupportQuality result;
    if (polygon.points.size() < 3 || maximumEvaluatedCells == 0) {
        return result;
    }
    const PolygonCellCoverage coverage = rasterizePolygonCellCoverage(
        grid, polygon, maximumEvaluatedCells);
    result.evaluatedCells = coverage.evaluatedCells;
    result.budgetExceeded = coverage.budgetExceeded;
    for (int localY = 0; localY < coverage.height; ++localY) {
        for (int localX = 0; localX < coverage.width; ++localX) {
            const std::size_t localIndex =
                static_cast<std::size_t>(localY) *
                    static_cast<std::size_t>(coverage.width) +
                static_cast<std::size_t>(localX);
            if (coverage.covered[localIndex] == 0) {
                continue;
            }
            const int x = coverage.firstX + localX;
            const int y = coverage.firstY + localY;
            ++result.insideCells;
            const double value = grid.value(x, y);
            const bool finite = std::isfinite(value);
            if (finite && strongSupportMask.get(x, y) &&
                value >= strongSupportThreshold) {
                ++result.strongSupportedCells;
            }
            if (finite && weakSupportMask.get(x, y) &&
                value >= weakSupportThreshold) {
                ++result.weakSupportedCells;
            }
            if (!finite || value <= backgroundThreshold) {
                ++result.backgroundCells;
            }
        }
    }
    return result;
}

double polygonBoundaryRoughness(const Polygon& polygon) {
    double weightedEnergy = 0.0;
    double totalPerimeter = 0.0;
    const auto addRing = [&](const std::vector<Point>& ring) {
        const double perimeter = polygonPerimeter(ring);
        weightedEnergy += ringCurvatureEnergy(ring) * perimeter;
        totalPerimeter += perimeter;
    };
    addRing(polygon.points);
    for (const std::vector<Point>& hole : polygon.holes) {
        addRing(hole);
    }
    return totalPerimeter > 1e-12
        ? weightedEnergy / totalPerimeter
        : 0.0;
}

void refreshPolygonGeometry(Polygon& polygon) {
    polygon.area = polygonArea(polygon.points);
    polygon.perimeter = polygonPerimeter(polygon.points);
    for (const std::vector<Point>& hole : polygon.holes) {
        polygon.area -= polygonArea(hole);
        polygon.perimeter += polygonPerimeter(hole);
    }
    polygon.area = std::max(0.0, polygon.area);
}

} // namespace fpt
