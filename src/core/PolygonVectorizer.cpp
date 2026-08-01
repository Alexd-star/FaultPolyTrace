#include "PolygonVectorizer.h"

#include "PolygonQualityMetrics.h"
#include "Topology.h"
#include "TraceOptions.h"
#include "TraceResult.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace fpt {
namespace polygon_vectorizer_detail {

struct Cell {
    int x = 0;
    int y = 0;
};

struct ComponentStats {
    int minX = 0;
    int minY = 0;
    int maxX = 0;
    int maxY = 0;
    int width = 0;
    int height = 0;
    double aspect = 1.0;
};

struct Vertex {
    int x = 0;
    int y = 0;

    bool operator<(const Vertex& other) const {
        return x == other.x ? y < other.y : x < other.x;
    }
    bool operator==(const Vertex& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Vertex& other) const {
        return !(*this == other);
    }
};

struct Edge {
    Vertex from;
    Vertex to;
};

Point toWorldPoint(const Grid2D& grid, const Vertex& vertex) {
    return {grid.worldX(vertex.x), grid.worldY(vertex.y)};
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
    double t = ((point.x - first.x) * dx +
                (point.y - first.y) * dy) / lengthSquared;
    t = std::max(0.0, std::min(1.0, t));
    const double projectedX = first.x + t * dx;
    const double projectedY = first.y + t * dy;
    const double px = point.x - projectedX;
    const double py = point.y - projectedY;
    return px * px + py * py;
}


double signedPolygonArea(const std::vector<Point>& points) {
    if (points.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point& a = points[i];
        const Point& b = points[(i + 1) % points.size()];
        sum += a.x * b.y - b.x * a.y;
    }
    return 0.5 * sum;
}

bool pointInRing(const Point& point, const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return false;
    }

    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Point& a = ring[i];
        const Point& b = ring[j];
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) / ((b.y - a.y) + 1e-30) + a.x);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

double pointToRingDistance(const Point& point, const std::vector<Point>& ring) {
    if (pointInRing(point, ring)) {
        return 0.0;
    }
    if (ring.size() < 2) {
        return std::numeric_limits<double>::infinity();
    }

    double best = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Point& a = ring[i];
        const Point& b = ring[(i + 1) % ring.size()];
        best = std::min(best, pointSegmentDistanceSquared(point, a, b));
    }
    return std::sqrt(best);
}

bool polygonPreservesCoverage(
    const Grid2D& grid,
    const std::vector<Point>& ring,
    const std::vector<Cell>& cells,
    double minPreserveValue,
    double preserveMargin) {
    if (ring.size() < 3) {
        return false;
    }
    const double margin = std::max(0.0, preserveMargin);
    int protectedCellCount = 0;
    for (const Cell& cell : cells) {
        if (grid.value(cell.x, cell.y) < minPreserveValue) {
            continue;
        }
        ++protectedCellCount;
        const Point center{
            grid.worldX(cell.x + 0.5),
            grid.worldY(cell.y + 0.5)
        };
        if (pointToRingDistance(center, ring) > margin) {
            return false;
        }
    }
    if (protectedCellCount == 0) {
        return true;
    }
    return true;
}

struct BoundaryRingSet {
    std::vector<Point> exterior;
    std::vector<std::vector<Point>> holes;
};

std::vector<BoundaryRingSet> selectBoundaryRingSets(
    std::vector<std::vector<Point>> rings,
    int& suppressedInternalRings) {
    std::vector<BoundaryRingSet> ringSets;
    if (rings.empty()) {
        return ringSets;
    }

    std::vector<double> areas(rings.size(), 0.0);
    for (std::size_t i = 0; i < rings.size(); ++i) {
        areas[i] = std::abs(signedPolygonArea(rings[i]));
    }

    std::vector<int> parent(rings.size(), -1);
    std::vector<int> depth(rings.size(), 0);
    std::vector<unsigned char> valid(rings.size(), 1);
    for (std::size_t i = 0; i < rings.size(); ++i) {
        if (rings[i].size() < 3 || areas[i] <= 1e-9) {
            ++suppressedInternalRings;
            valid[i] = 0;
            continue;
        }

        const Point probe = rings[i].front();
        double bestParentArea = std::numeric_limits<double>::infinity();
        for (std::size_t j = 0; j < rings.size(); ++j) {
            if (i == j || rings[j].size() < 3 || areas[j] <= areas[i]) {
                continue;
            }
            if (pointInRing(probe, rings[j])) {
                if (areas[j] < bestParentArea) {
                    bestParentArea = areas[j];
                    parent[i] = static_cast<int>(j);
                }
            }
        }
    }

    for (std::size_t i = 0; i < rings.size(); ++i) {
        if (!valid[i]) {
            continue;
        }
        int cursor = parent[i];
        while (cursor >= 0) {
            ++depth[i];
            cursor = parent[static_cast<std::size_t>(cursor)];
        }
    }

    std::vector<int> exteriorSetIndex(rings.size(), -1);
    for (std::size_t i = 0; i < rings.size(); ++i) {
        if (!valid[i]) {
            continue;
        }
        if (depth[i] == 0 || (depth[i] % 2) == 0) {
            exteriorSetIndex[i] = static_cast<int>(ringSets.size());
            BoundaryRingSet set;
            set.exterior = std::move(rings[i]);
            ringSets.push_back(std::move(set));
        }
    }

    for (std::size_t i = 0; i < rings.size(); ++i) {
        if (!valid[i] || (depth[i] % 2) != 1) {
            continue;
        }
        int ancestor = parent[i];
        while (ancestor >= 0 && (depth[static_cast<std::size_t>(ancestor)] % 2) != 0) {
            ancestor = parent[static_cast<std::size_t>(ancestor)];
        }
        if (ancestor < 0) {
            ++suppressedInternalRings;
            continue;
        }
        const int setIndex = exteriorSetIndex[static_cast<std::size_t>(ancestor)];
        if (setIndex < 0 || setIndex >= static_cast<int>(ringSets.size())) {
            ++suppressedInternalRings;
            continue;
        }
        ringSets[static_cast<std::size_t>(setIndex)].holes.push_back(std::move(rings[i]));
    }

    return ringSets;
}

ComponentStats computeComponentStats(const std::vector<Cell>& cells) {
    ComponentStats stats;
    if (cells.empty()) {
        return stats;
    }

    stats.minX = stats.maxX = cells.front().x;
    stats.minY = stats.maxY = cells.front().y;
    for (const Cell& cell : cells) {
        stats.minX = std::min(stats.minX, cell.x);
        stats.maxX = std::max(stats.maxX, cell.x);
        stats.minY = std::min(stats.minY, cell.y);
        stats.maxY = std::max(stats.maxY, cell.y);
    }
    stats.width = stats.maxX - stats.minX + 1;
    stats.height = stats.maxY - stats.minY + 1;
    stats.aspect = static_cast<double>(std::max(stats.width, stats.height)) /
        static_cast<double>(std::max(1, std::min(stats.width, stats.height)));
    return stats;
}

bool shouldKeepPolygon(
    double area,
    int cellCount,
    double meanValue,
    double maxValue,
    const ComponentStats& stats,
    const TraceOptions& options,
    double effectiveThreshold) {
    if (area >= options.minArea) {
        return true;
    }
    if (!options.adaptiveAreaFilter) {
        return false;
    }
    if (cellCount < options.adaptiveAreaMinCells) {
        return false;
    }

    const double meanFloor = std::max(0.0, effectiveThreshold * options.adaptiveAreaMinMeanRatio);
    const double maxFloor = std::max(0.0, effectiveThreshold * options.adaptiveAreaMinMaxRatio);
    const bool strongEnough = maxValue >= maxFloor || meanValue >= effectiveThreshold;
    const bool lineLike = stats.aspect >= options.adaptiveAreaMinAspect ||
        std::max(stats.width, stats.height) >= options.residualTraceMinLength;
    return lineLike && strongEnough && meanValue >= meanFloor;
}

bool finalizeAndAppendPolygon(
    const Grid2D& grid,
    const std::vector<Cell>& cells,
    double sum,
    double maxValue,
    std::vector<Point> ring,
    std::vector<std::vector<Point>> holes,
    const ComponentStats& stats,
    const TraceOptions& options,
    double keepThreshold,
    TraceResult& result,
    int& nextId) {
    const double inputSignedArea = signedPolygonArea(ring);
    if (std::abs(inputSignedArea) <= 1e-9) {
        return false;
    }
    if (inputSignedArea < 0.0) {
        std::reverse(ring.begin(), ring.end());
    }

    Polygon polygon;
    polygon.id = nextId++;
    polygon.points = std::move(ring);
    polygon.holes = std::move(holes);
    for (std::vector<Point>& hole : polygon.holes) {
        const double holeSignedArea = signedPolygonArea(hole);
        if (std::abs(holeSignedArea) <= 1e-9) {
            hole.clear();
        } else if (holeSignedArea > 0.0) {
            std::reverse(hole.begin(), hole.end());
        }
    }
    polygon.holes.erase(
        std::remove_if(
            polygon.holes.begin(),
            polygon.holes.end(),
            [](const std::vector<Point>& hole) { return hole.size() < 3; }),
        polygon.holes.end());
    const double minPreserveValue = std::max(0.0, keepThreshold * options.coveragePreserveMinValueRatio);
    if (options.simplifyTolerance > 0.0) {
        std::vector<Point> simplified = simplifyRingDouglasPeucker(polygon.points, options.simplifyTolerance);
        if (options.preserveCoverageAfterSmoothing &&
            !polygonPreservesCoverage(grid, simplified, cells, minPreserveValue, options.coveragePreserveMargin)) {
            Polygon coverageFallback = polygon;
            if (polygonHasTopologyConflict(coverageFallback, result.polygons, true)) {
                Polygon topologySafeCandidate = polygon;
                topologySafeCandidate.points = std::move(simplified);
                if (!polygonHasTopologyConflict(topologySafeCandidate, result.polygons, true)) {
                    polygon.points = std::move(topologySafeCandidate.points);
                    ++result.coveragePreserveTopologyOverridePolygons;
                } else {
                    ++result.coveragePreserveSimplifyFallbackPolygons;
                }
            } else {
                ++result.coveragePreserveSimplifyFallbackPolygons;
            }
        } else {
            polygon.points = std::move(simplified);
        }
    }
    if (options.smoothIterations > 0) {
        std::vector<Point> smoothed = smoothRingChaikin(polygon.points, options.smoothIterations);
        if (options.preserveCoverageAfterSmoothing &&
            !polygonPreservesCoverage(grid, smoothed, cells, minPreserveValue, options.coveragePreserveMargin)) {
            Polygon coverageFallback = polygon;
            if (polygonHasTopologyConflict(coverageFallback, result.polygons, true)) {
                Polygon topologySafeCandidate = polygon;
                topologySafeCandidate.points = std::move(smoothed);
                if (!polygonHasTopologyConflict(topologySafeCandidate, result.polygons, true)) {
                    polygon.points = std::move(topologySafeCandidate.points);
                    ++result.coveragePreserveTopologyOverridePolygons;
                } else {
                    ++result.coveragePreserveSmoothFallbackPolygons;
                }
            } else {
                ++result.coveragePreserveSmoothFallbackPolygons;
            }
        } else {
            polygon.points = std::move(smoothed);
        }
    }
    std::vector<std::vector<Point>> topologySafeHoles;
    topologySafeHoles.reserve(polygon.holes.size());
    for (std::vector<Point>& hole : polygon.holes) {
        bool conflict = hole.empty() || !pointInRing(hole.front(), polygon.points) ||
            ringBoundariesConflict(polygon.points, hole, true);
        if (!conflict) {
            for (const std::vector<Point>& accepted : topologySafeHoles) {
                if (ringBoundariesConflict(accepted, hole, true)) {
                    conflict = true;
                    break;
                }
            }
        }
        if (conflict) {
            ++result.suppressedInternalRings;
        } else {
            topologySafeHoles.push_back(std::move(hole));
        }
    }
    polygon.holes = std::move(topologySafeHoles);
    polygon.area = polygonArea(polygon.points);
    polygon.perimeter = polygonPerimeter(polygon.points);
    for (const std::vector<Point>& hole : polygon.holes) {
        polygon.area -= polygonArea(hole);
        polygon.perimeter += polygonPerimeter(hole);
    }
    polygon.area = std::max(0.0, polygon.area);
    polygon.cellCount = static_cast<int>(cells.size());
    polygon.meanValue = cells.empty() ? 0.0 : sum / static_cast<double>(cells.size());
    polygon.maxValue = maxValue;
    polygon.kept = shouldKeepPolygon(
        polygon.area,
        polygon.cellCount,
        polygon.meanValue,
        polygon.maxValue,
        stats,
        options,
        keepThreshold);
    result.polygons.push_back(std::move(polygon));
    return result.polygons.back().kept;
}

std::vector<Point> removeCollinear(std::vector<Point> points) {
    if (points.size() < 3) {
        return points;
    }

    std::vector<Point> compact;
    compact.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point& prev = points[(i + points.size() - 1) % points.size()];
        const Point& current = points[i];
        const Point& next = points[(i + 1) % points.size()];
        const double ax = current.x - prev.x;
        const double ay = current.y - prev.y;
        const double bx = next.x - current.x;
        const double by = next.y - current.y;
        const double crossValue = ax * by - ay * bx;
        if (std::abs(crossValue) > 1e-12) {
            compact.push_back(current);
        }
    }
    return compact.size() >= 3 ? compact : points;
}

std::vector<std::vector<Vertex>> splitSelfTouchingVertexRing(const std::vector<Vertex>& vertices) {
    std::vector<std::vector<Vertex>> loops;
    if (vertices.size() < 3) {
        return loops;
    }

    std::vector<Vertex> stack;
    std::map<Vertex, std::size_t> positionByVertex;

    auto pushVertex = [&stack, &positionByVertex](const Vertex& vertex) {
        positionByVertex[vertex] = stack.size();
        stack.push_back(vertex);
    };

    for (std::size_t i = 0; i <= vertices.size(); ++i) {
        const Vertex vertex = (i == vertices.size()) ? vertices.front() : vertices[i];
        if (stack.empty()) {
            pushVertex(vertex);
            continue;
        }

        auto repeat = positionByVertex.find(vertex);
        if (repeat == positionByVertex.end()) {
            pushVertex(vertex);
            continue;
        }

        const std::size_t start = repeat->second;
        if (stack.size() - start >= 3) {
            loops.emplace_back(stack.begin() + static_cast<std::ptrdiff_t>(start), stack.end());
        }
        for (std::size_t eraseIndex = start + 1; eraseIndex < stack.size(); ++eraseIndex) {
            positionByVertex.erase(stack[eraseIndex]);
        }
        stack.resize(start + 1);
    }

    if (loops.empty()) {
        loops.push_back(vertices);
    }
    return loops;
}

std::vector<std::vector<Point>> traceBoundaryRings(const Grid2D& grid, const std::vector<Cell>& cells) {
    if (cells.empty()) {
        return {};
    }
    int minX = cells.front().x;
    int maxX = cells.front().x;
    int minY = cells.front().y;
    int maxY = cells.front().y;
    for (const Cell& cell : cells) {
        minX = std::min(minX, cell.x);
        maxX = std::max(maxX, cell.x);
        minY = std::min(minY, cell.y);
        maxY = std::max(maxY, cell.y);
    }
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    std::vector<unsigned char> componentMask(
        static_cast<std::size_t>(width * height), 0);
    for (const Cell& cell : cells) {
        componentMask[static_cast<std::size_t>(
            (cell.y - minY) * width + cell.x - minX)] = 1;
    }
    const auto contains = [&](int x, int y) {
        if (x < minX || x > maxX || y < minY || y > maxY) {
            return false;
        }
        return componentMask[static_cast<std::size_t>(
            (y - minY) * width + x - minX)] != 0;
    };

    std::vector<Edge> edges;
    edges.reserve(cells.size() * 4);
    for (const Cell& cell : cells) {
        const int x = cell.x;
        const int y = cell.y;

        if (!contains(x, y - 1)) {
            edges.push_back({{x, y}, {x + 1, y}});
        }
        if (!contains(x + 1, y)) {
            edges.push_back({{x + 1, y}, {x + 1, y + 1}});
        }
        if (!contains(x, y + 1)) {
            edges.push_back({{x + 1, y + 1}, {x, y + 1}});
        }
        if (!contains(x - 1, y)) {
            edges.push_back({{x, y + 1}, {x, y}});
        }
    }

    std::map<Vertex, std::vector<Vertex>> nextByStart;
    for (const Edge& edge : edges) {
        nextByStart[edge.from].push_back(edge.to);
    }
    for (auto& item : nextByStart) {
        std::sort(item.second.begin(), item.second.end(), [](const Vertex& a, const Vertex& b) {
            if (a.x == b.x) {
                return a.y < b.y;
            }
            return a.x < b.x;
        });
    }

    std::vector<std::vector<Point>> rings;
    while (!nextByStart.empty()) {
        auto startIt = nextByStart.begin();
        const Vertex start = startIt->first;
        Vertex current = start;
        std::vector<Vertex> ringVertices;
        ringVertices.push_back(current);

        int guard = 0;
        const int maxSteps = static_cast<int>(edges.size()) + 4;
        while (guard++ < maxSteps) {
            auto it = nextByStart.find(current);
            if (it == nextByStart.end() || it->second.empty()) {
                break;
            }

            const Vertex next = it->second.back();
            it->second.pop_back();
            if (it->second.empty()) {
                nextByStart.erase(it);
            }

            current = next;
            if (current == start) {
                break;
            }
            ringVertices.push_back(current);
        }

        if (current != start || ringVertices.size() < 3) {
            continue;
        }

        std::vector<std::vector<Vertex>> simpleVertexLoops = splitSelfTouchingVertexRing(ringVertices);
        for (const std::vector<Vertex>& vertexLoop : simpleVertexLoops) {
            std::vector<Point> ring;
            ring.reserve(vertexLoop.size());
            for (const Vertex& vertex : vertexLoop) {
                ring.push_back(toWorldPoint(grid, vertex));
            }
            ring = removeCollinear(std::move(ring));
            if (ring.size() >= 3) {
                rings.push_back(std::move(ring));
            }
        }
    }

    return rings;
}

BinaryMask rasterizeKeptPolygonCoverage(
    const Grid2D& grid,
    const std::vector<Polygon>& polygons,
    const BinaryMask* candidateMask) {
    BinaryMask covered(grid.nx(), grid.ny());
    for (const Polygon& polygon : polygons) {
        if (!polygon.kept || polygon.points.size() < 3) {
            continue;
        }
        double minWorldX = polygon.points.front().x;
        double maxWorldX = polygon.points.front().x;
        double minWorldY = polygon.points.front().y;
        double maxWorldY = polygon.points.front().y;
        for (const Point& point : polygon.points) {
            minWorldX = std::min(minWorldX, point.x);
            maxWorldX = std::max(maxWorldX, point.x);
            minWorldY = std::min(minWorldY, point.y);
            maxWorldY = std::max(maxWorldY, point.y);
        }
        const int minX = std::max(
            0,
            static_cast<int>(std::floor(
                (minWorldX - grid.sx()) / grid.dx() - 0.5)) - 1);
        const int maxX = std::min(
            grid.nx() - 1,
            static_cast<int>(std::ceil(
                (maxWorldX - grid.sx()) / grid.dx() - 0.5)) + 1);
        const int minY = std::max(
            0,
            static_cast<int>(std::floor(
                (minWorldY - grid.sy()) / grid.dy() - 0.5)) - 1);
        const int maxY = std::min(
            grid.ny() - 1,
            static_cast<int>(std::ceil(
                (maxWorldY - grid.sy()) / grid.dy() - 0.5)) + 1);
        const PolygonCellCoverage polygonCoverage =
            rasterizePolygonCellCoverage(grid, polygon, grid.size());
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if ((!candidateMask || candidateMask->get(x, y)) &&
                    polygonCoverage.get(x, y)) {
                    covered.set(x, y, true);
                }
            }
        }
    }
    return covered;
}

std::vector<std::vector<Point>> traceSubcellConnectedBoundaryRings(
    const Grid2D& grid,
    const std::vector<Cell>& cells,
    const ComponentStats& stats) {
    constexpr int kScale = 4;
    const int sourceWidth = stats.maxX - stats.minX + 1;
    const int sourceHeight = stats.maxY - stats.minY + 1;
    GridMetadata metadata;
    metadata.nx = sourceWidth * kScale;
    metadata.ny = sourceHeight * kScale;
    metadata.sx = grid.worldX(stats.minX);
    metadata.sy = grid.worldY(stats.minY);
    metadata.dx = grid.dx() / static_cast<double>(kScale);
    metadata.dy = grid.dy() / static_cast<double>(kScale);
    Grid2D subcellGrid(metadata);

    std::vector<unsigned char> sourceMembership(
        static_cast<std::size_t>(sourceWidth * sourceHeight), 0);
    for (const Cell& cell : cells) {
        sourceMembership[static_cast<std::size_t>(
            (cell.y - stats.minY) * sourceWidth +
            cell.x - stats.minX)] = 1;
    }
    const auto containsSource = [&](int x, int y) {
        return x >= stats.minX && x <= stats.maxX &&
            y >= stats.minY && y <= stats.maxY &&
            sourceMembership[static_cast<std::size_t>(
                (y - stats.minY) * sourceWidth +
                x - stats.minX)] != 0;
    };

    std::vector<unsigned char> subcellMembership(
        static_cast<std::size_t>(metadata.nx * metadata.ny), 0);
    std::vector<Cell> subcells;
    subcells.reserve(cells.size() * kScale * kScale + cells.size());
    const auto addSubcell = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= metadata.nx || y >= metadata.ny) {
            return;
        }
        const std::size_t index = static_cast<std::size_t>(
            y * metadata.nx + x);
        if (!subcellMembership[index]) {
            subcellMembership[index] = 1;
            subcells.push_back({x, y});
        }
    };

    for (const Cell& cell : cells) {
        const int baseX = (cell.x - stats.minX) * kScale;
        const int baseY = (cell.y - stats.minY) * kScale;
        for (int oy = 0; oy < kScale; ++oy) {
            for (int ox = 0; ox < kScale; ++ox) {
                addSubcell(baseX + ox, baseY + oy);
            }
        }
    }

    const auto finiteValue = [&grid](int x, int y) {
        if (!grid.inBounds(x, y)) {
            return -std::numeric_limits<double>::infinity();
        }
        const double value = grid.value(x, y);
        return std::isfinite(value)
            ? value
            : -std::numeric_limits<double>::infinity();
    };
    for (const Cell& cell : cells) {
        for (const int directionY : {-1, 1}) {
            const int diagonalX = cell.x + 1;
            const int diagonalY = cell.y + directionY;
            if (!containsSource(diagonalX, diagonalY) ||
                containsSource(cell.x + 1, cell.y) ||
                containsSource(cell.x, cell.y + directionY)) {
                continue;
            }
            const int baseX = (cell.x - stats.minX) * kScale;
            const int baseY = (cell.y - stats.minY) * kScale;
            const int cornerX = baseX + kScale;
            const int cornerY = baseY + (directionY > 0 ? kScale : 0);
            const double horizontalValue =
                finiteValue(cell.x + 1, cell.y);
            const double verticalValue =
                finiteValue(cell.x, cell.y + directionY);
            if (horizontalValue >= verticalValue) {
                addSubcell(
                    cornerX,
                    cornerY + (directionY > 0 ? -1 : 0));
            } else {
                addSubcell(
                    cornerX - 1,
                    cornerY + (directionY > 0 ? 0 : -1));
            }
        }
    }
    return traceBoundaryRings(subcellGrid, subcells);
}

std::vector<Point> convexCellEnvelope(
    const Grid2D& grid,
    const std::vector<Cell>& cells) {
    std::vector<Point> points;
    points.reserve(cells.size() * 4);
    for (const Cell& cell : cells) {
        points.push_back({grid.worldX(cell.x), grid.worldY(cell.y)});
        points.push_back({grid.worldX(cell.x + 1), grid.worldY(cell.y)});
        points.push_back({grid.worldX(cell.x + 1), grid.worldY(cell.y + 1)});
        points.push_back({grid.worldX(cell.x), grid.worldY(cell.y + 1)});
    }
    std::sort(points.begin(), points.end(), [](const Point& first, const Point& second) {
        return std::tie(first.x, first.y) < std::tie(second.x, second.y);
    });
    points.erase(
        std::unique(
            points.begin(), points.end(),
            [](const Point& first, const Point& second) {
                return first.x == second.x && first.y == second.y;
            }),
        points.end());
    if (points.size() < 3) {
        return points;
    }
    const auto cross = [](const Point& origin, const Point& a, const Point& b) {
        return (a.x - origin.x) * (b.y - origin.y) -
            (a.y - origin.y) * (b.x - origin.x);
    };
    std::vector<Point> hull;
    hull.reserve(points.size() * 2);
    for (const Point& point : points) {
        while (hull.size() >= 2 &&
               cross(hull[hull.size() - 2], hull.back(), point) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(point);
    }
    const std::size_t lowerSize = hull.size();
    for (auto iterator = points.rbegin() + 1; iterator != points.rend(); ++iterator) {
        while (hull.size() > lowerSize &&
               cross(hull[hull.size() - 2], hull.back(), *iterator) <= 0.0) {
            hull.pop_back();
        }
        hull.push_back(*iterator);
    }
    if (hull.size() > 1 &&
        hull.front().x == hull.back().x &&
        hull.front().y == hull.back().y) {
        hull.pop_back();
    }
    return hull;
}

void appendPolygonsFromMask(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId) {
    std::vector<unsigned char> visited(static_cast<std::size_t>(grid.nx() * grid.ny()), 0);
    const std::vector<unsigned char>& active = activeMask.values();

    auto indexOf = [&grid](int x, int y) {
        return y * grid.nx() + x;
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    for (int y0 = 0; y0 < grid.ny(); ++y0) {
        for (int x0 = 0; x0 < grid.nx(); ++x0) {
            const int startIdx = indexOf(x0, y0);
            if (!active[static_cast<std::size_t>(startIdx)] || visited[static_cast<std::size_t>(startIdx)]) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            queue.push({x0, y0});
            visited[static_cast<std::size_t>(startIdx)] = 1;

            double sum = 0.0;
            double maxValue = -std::numeric_limits<double>::infinity();

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                const double v = grid.value(cell.x, cell.y);
                sum += v;
                maxValue = std::max(maxValue, v);

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    const int neighborIdx = indexOf(nxCell, nyCell);
                    const std::size_t pos = static_cast<std::size_t>(neighborIdx);
                    if (active[pos] && !visited[pos]) {
                        visited[pos] = 1;
                        queue.push({nxCell, nyCell});
                    }
                }
            }

            if (static_cast<int>(cells.size()) < options.minCells) {
                continue;
            }

            const ComponentStats stats = computeComponentStats(cells);
            std::vector<BoundaryRingSet> rings = selectBoundaryRingSets(
                traceBoundaryRings(grid, cells),
                result.suppressedInternalRings);
            for (BoundaryRingSet& ring : rings) {
                finalizeAndAppendPolygon(
                    grid,
                    cells,
                    sum,
                    maxValue,
                    std::move(ring.exterior),
                    std::move(ring.holes),
                    stats,
                    options,
                    result.effectiveThreshold,
                    result,
                    nextId);
            }
        }
    }
}

[[maybe_unused]] void appendPolygonsFromMaskBounds(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    int minX,
    int minY,
    int maxX,
    int maxY,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId) {
    minX = std::max(0, minX);
    minY = std::max(0, minY);
    maxX = std::min(grid.nx() - 1, maxX);
    maxY = std::min(grid.ny() - 1, maxY);
    if (minX > maxX || minY > maxY) {
        return;
    }

    const int localWidth = maxX - minX + 1;
    const int localHeight = maxY - minY + 1;
    std::vector<unsigned char> visited(static_cast<std::size_t>(localWidth * localHeight), 0);

    auto localIndex = [localWidth, minX, minY](int x, int y) {
        return (y - minY) * localWidth + (x - minX);
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    for (int y0 = minY; y0 <= maxY; ++y0) {
        for (int x0 = minX; x0 <= maxX; ++x0) {
            const int startIdx = localIndex(x0, y0);
            if (!activeMask.get(x0, y0) || visited[static_cast<std::size_t>(startIdx)]) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            queue.push({x0, y0});
            visited[static_cast<std::size_t>(startIdx)] = 1;

            double sum = 0.0;
            double maxValue = -std::numeric_limits<double>::infinity();

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                const double v = grid.value(cell.x, cell.y);
                sum += v;
                maxValue = std::max(maxValue, v);

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (nxCell < minX || nyCell < minY || nxCell > maxX || nyCell > maxY) {
                        continue;
                    }
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    const int neighborIdx = localIndex(nxCell, nyCell);
                    const std::size_t pos = static_cast<std::size_t>(neighborIdx);
                    if (activeMask.get(nxCell, nyCell) && !visited[pos]) {
                        visited[pos] = 1;
                        queue.push({nxCell, nyCell});
                    }
                }
            }

            if (static_cast<int>(cells.size()) < options.minCells) {
                continue;
            }

            const ComponentStats stats = computeComponentStats(cells);
            std::vector<BoundaryRingSet> rings = selectBoundaryRingSets(
                traceBoundaryRings(grid, cells),
                result.suppressedInternalRings);
            for (BoundaryRingSet& ring : rings) {
                finalizeAndAppendPolygon(
                    grid,
                    cells,
                    sum,
                    maxValue,
                    std::move(ring.exterior),
                    std::move(ring.holes),
                    stats,
                    options,
                    result.effectiveThreshold,
                    result,
                    nextId);
            }
        }
    }
}

void appendPolygonsFromLocalMask(
    const Grid2D& grid,
    const std::vector<unsigned char>& active,
    int minX,
    int minY,
    int localWidth,
    int localHeight,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId,
    BinaryMask* keptCoverageMask = nullptr) {
    if (localWidth <= 0 || localHeight <= 0 ||
        active.size() != static_cast<std::size_t>(localWidth * localHeight)) {
        return;
    }

    std::vector<unsigned char> visited(static_cast<std::size_t>(localWidth * localHeight), 0);
    auto localIndex = [localWidth](int x, int y) {
        return y * localWidth + x;
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    for (int y0 = 0; y0 < localHeight; ++y0) {
        for (int x0 = 0; x0 < localWidth; ++x0) {
            const int startIdx = localIndex(x0, y0);
            if (!active[static_cast<std::size_t>(startIdx)] || visited[static_cast<std::size_t>(startIdx)]) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            queue.push({x0, y0});
            visited[static_cast<std::size_t>(startIdx)] = 1;

            double sum = 0.0;
            double maxValue = -std::numeric_limits<double>::infinity();

            while (!queue.empty()) {
                const Cell localCell = queue.front();
                queue.pop();
                const int gx = minX + localCell.x;
                const int gy = minY + localCell.y;
                if (!grid.inBounds(gx, gy)) {
                    continue;
                }
                cells.push_back({gx, gy});
                const double v = grid.value(gx, gy);
                sum += v;
                maxValue = std::max(maxValue, v);

                for (const auto& offset : offsets) {
                    const int nxCell = localCell.x + offset[0];
                    const int nyCell = localCell.y + offset[1];
                    if (nxCell < 0 || nyCell < 0 || nxCell >= localWidth || nyCell >= localHeight) {
                        continue;
                    }
                    const int neighborIdx = localIndex(nxCell, nyCell);
                    const std::size_t pos = static_cast<std::size_t>(neighborIdx);
                    if (active[pos] && !visited[pos]) {
                        visited[pos] = 1;
                        queue.push({nxCell, nyCell});
                    }
                }
            }

            if (static_cast<int>(cells.size()) < options.minCells) {
                continue;
            }

            const ComponentStats stats = computeComponentStats(cells);
            std::vector<BoundaryRingSet> rings = selectBoundaryRingSets(
                traceBoundaryRings(grid, cells),
                result.suppressedInternalRings);
            for (BoundaryRingSet& ring : rings) {
                const bool kept = finalizeAndAppendPolygon(
                    grid,
                    cells,
                    sum,
                    maxValue,
                    std::move(ring.exterior),
                    std::move(ring.holes),
                    stats,
                    options,
                    result.effectiveThreshold,
                    result,
                    nextId);
                if (kept && keptCoverageMask) {
                    for (const Cell& cell : cells) {
                        keptCoverageMask->set(cell.x, cell.y, true);
                    }
                }
            }
        }
    }
}

void appendResidualTracePolygons(
    const Grid2D& grid,
    const BinaryMask& residualMask,
    const std::vector<int>& groupOwners,
    const BinaryMask& alreadyCoveredMask,
    const BinaryMask& candidateCoveredMask,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId) {
    if (!options.residualTrace || options.residualPercentile <= 0.0) {
        return;
    }

    const BinaryMask coveredBaseMask = candidateCoveredMask.empty() ? alreadyCoveredMask : candidateCoveredMask;
    const BinaryMask coveredGuardMask = options.residualCoveredGuardRadius > 0
        ? dilate(coveredBaseMask, options.residualCoveredGuardRadius)
        : coveredBaseMask;
    std::vector<unsigned char> candidate(static_cast<std::size_t>(grid.nx() * grid.ny()), 0);
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (residualMask.get(x, y) && !coveredGuardMask.get(x, y)) {
                const std::size_t pos = static_cast<std::size_t>(y * grid.nx() + x);
                const int owner = groupOwners.empty() ? 1 : groupOwners[pos];
                const bool usable = owner > 0 || (options.residualTraceDetached && owner == -1);
                if (usable) {
                    candidate[pos] = 1;
                }
            }
        }
    }

    std::vector<unsigned char> visited(static_cast<std::size_t>(grid.nx() * grid.ny()), 0);
    auto indexOf = [&grid](int x, int y) {
        return y * grid.nx() + x;
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    for (int y0 = 0; y0 < grid.ny(); ++y0) {
        for (int x0 = 0; x0 < grid.nx(); ++x0) {
            const std::size_t startPos = static_cast<std::size_t>(indexOf(x0, y0));
            if (!candidate[startPos] || visited[startPos]) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            queue.push({x0, y0});
            visited[startPos] = 1;
            double sum = 0.0;
            double maxValue = -std::numeric_limits<double>::infinity();
            int ownerId = groupOwners.empty() ? 1 : groupOwners[startPos];
            bool touchesCovered = false;

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                const double v = grid.value(cell.x, cell.y);
                sum += v;
                maxValue = std::max(maxValue, v);

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    if (alreadyCoveredMask.get(nxCell, nyCell)) {
                        touchesCovered = true;
                    }
                    const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                    if (!candidate[pos] || visited[pos]) {
                        continue;
                    }
                    if (!groupOwners.empty() && groupOwners[pos] != ownerId) {
                        continue;
                    }
                    visited[pos] = 1;
                    queue.push({nxCell, nyCell});
                }
            }

            ++result.residualTraceCandidateComponents;
            const int traceMinimumCells = std::max(
                options.residualMinCells,
                options.residualDetachedMinCells);
            if (static_cast<int>(cells.size()) < traceMinimumCells) {
                ++result.residualTraceSizeRejectedComponents;
                continue;
            }
            if (options.residualTraceMaxCells > 0 &&
                static_cast<int>(cells.size()) > options.residualTraceMaxCells) {
                ++result.residualTraceSizeRejectedComponents;
                continue;
            }

            const ComponentStats stats = computeComponentStats(cells);
            const int longestAxis = std::max(stats.width, stats.height);
            const double meanValue = cells.empty() ? 0.0 : sum / static_cast<double>(cells.size());
            const bool lineLike =
                stats.aspect >= options.residualDetachedMinAspect &&
                longestAxis >= std::max(
                    options.residualTraceMinLength,
                    options.residualDetachedMinLength);
            const bool strongEnough =
                maxValue >= result.effectiveThreshold *
                    options.residualDetachedMinMaxRatio &&
                meanValue >= result.effectiveThreshold *
                    options.residualDetachedMinMeanRatio;
            if (!lineLike) {
                ++result.residualTraceShapeRejectedComponents;
                continue;
            }
            if (!strongEnough) {
                ++result.residualTraceAttributeRejectedComponents;
                continue;
            }
            if (!touchesCovered && !options.residualTraceDetached) {
                ++result.residualTraceDetachedRejectedComponents;
                continue;
            }

            const std::size_t polygonCountBefore = result.polygons.size();
            std::vector<BoundaryRingSet> rings = selectBoundaryRingSets(
                traceSubcellConnectedBoundaryRings(grid, cells, stats),
                result.suppressedInternalRings);
            TraceOptions residualPolygonOptions = options;
            residualPolygonOptions.simplifyTolerance = 0.0;
            residualPolygonOptions.smoothIterations = 0;
            for (BoundaryRingSet& ring : rings) {
                finalizeAndAppendPolygon(
                    grid,
                    cells,
                    sum,
                    maxValue,
                    std::move(ring.exterior),
                    std::move(ring.holes),
                    stats,
                    residualPolygonOptions,
                    result.effectiveThreshold,
                    result,
                    nextId);
            }
            bool topologyConflict = false;
            bool fallbackAccepted = false;
            if (result.polygons.size() > polygonCountBefore) {
                for (std::size_t index = polygonCountBefore;
                     index < result.polygons.size(); ++index) {
                    if (result.polygons[index].kept &&
                        polygonHasTopologyConflict(
                            result.polygons[index], result.polygons, true)) {
                        topologyConflict = true;
                        break;
                    }
                }
                if (topologyConflict) {
                    result.polygons.resize(polygonCountBefore);
                    nextId = result.polygons.empty()
                        ? 1
                        : result.polygons.back().id + 1;
                }
                if (topologyConflict) {
                    std::vector<Point> fallback = convexCellEnvelope(grid, cells);
                    if (fallback.size() >= 3) {
                        finalizeAndAppendPolygon(
                            grid,
                            cells,
                            sum,
                            maxValue,
                            std::move(fallback),
                            {},
                            stats,
                            residualPolygonOptions,
                            result.effectiveThreshold,
                            result,
                            nextId);
                        if (result.polygons.size() > polygonCountBefore &&
                            result.polygons.back().kept &&
                            polygonHasTopologyConflict(
                                result.polygons.back(), result.polygons, true)) {
                            result.polygons.resize(polygonCountBefore);
                            nextId = result.polygons.empty()
                                ? 1
                                : result.polygons.back().id + 1;
                        } else if (result.polygons.size() > polygonCountBefore) {
                            fallbackAccepted = true;
                        }
                    }
                }
            }
            int keptAdded = 0;
            for (std::size_t index = polygonCountBefore;
                 index < result.polygons.size(); ++index) {
                keptAdded += result.polygons[index].kept ? 1 : 0;
            }
            if (keptAdded <= 0) {
                result.polygons.resize(polygonCountBefore);
                nextId = result.polygons.empty()
                    ? 1
                    : result.polygons.back().id + 1;
                if (topologyConflict) {
                    ++result.residualTraceTopologyRejectedComponents;
                } else {
                    ++result.residualTraceGeometryRejectedComponents;
                }
                continue;
            }
            ++result.residualTraceAcceptedComponents;
            result.residualTraceAcceptedPolygons += keptAdded;
            result.residualTraceAcceptedCells +=
                static_cast<int>(cells.size());
            result.residualTraceFallbackAcceptedComponents +=
                fallbackAccepted ? 1 : 0;
        }
    }
}
} // namespace polygon_vectorizer_detail

BinaryMask rasterizeKeptPolygonCoverage(
    const Grid2D& grid,
    const std::vector<Polygon>& polygons,
    const BinaryMask* candidateMask) {
    return polygon_vectorizer_detail::rasterizeKeptPolygonCoverage(
        grid, polygons, candidateMask);
}

void appendPolygonsFromMask(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId) {
    polygon_vectorizer_detail::appendPolygonsFromMask(
        grid, activeMask, options, result, nextId);
}

void appendPolygonsFromLocalMask(
    const Grid2D& grid,
    const std::vector<unsigned char>& active,
    int minX,
    int minY,
    int localWidth,
    int localHeight,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId,
    BinaryMask* keptCoverageMask) {
    polygon_vectorizer_detail::appendPolygonsFromLocalMask(
        grid, active, minX, minY, localWidth, localHeight,
        options, result, nextId, keptCoverageMask);
}

void appendResidualTracePolygons(
    const Grid2D& grid,
    const BinaryMask& residualMask,
    const std::vector<int>& groupOwners,
    const BinaryMask& alreadyCoveredMask,
    const BinaryMask& candidateCoveredMask,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId) {
    polygon_vectorizer_detail::appendResidualTracePolygons(
        grid, residualMask, groupOwners, alreadyCoveredMask,
        candidateCoveredMask, options, result, nextId);
}

} // namespace fpt
