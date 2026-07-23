#include "LocalPolygonQuality.h"

#include "Topology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <utility>

namespace fpt {
namespace {

struct LocalMask {
    int minX = 0;
    int minY = 0;
    int width = 0;
    int height = 0;
    std::vector<unsigned char> values;

    bool inBounds(int x, int y) const {
        return x >= 0 && y >= 0 && x < width && y < height;
    }

    bool get(int x, int y) const {
        return inBounds(x, y) &&
            values[static_cast<std::size_t>(y * width + x)] != 0;
    }

    void set(int x, int y, bool value) {
        if (inBounds(x, y)) {
            values[static_cast<std::size_t>(y * width + x)] = value ? 1 : 0;
        }
    }

    int count() const {
        return static_cast<int>(std::count(values.begin(), values.end(), static_cast<unsigned char>(1)));
    }
};

struct ProjectedCell {
    int localX = 0;
    int localY = 0;
    int bin = 0;
    double across = 0.0;
};

struct CandidateWindow {
    LocalQualityCandidateType type = LocalQualityCandidateType::LocalBulge;
    int minX = 0;
    int maxX = 0;
    int minY = 0;
    int maxY = 0;
    int candidateCells = 0;
    double baselineWidth = 0.0;
    double localWidth = 0.0;
    double widthRatio = 0.0;
    double spanRatio = 0.0;
    double supportRatio = 0.0;
};

struct Vertex {
    int x = 0;
    int y = 0;

    bool operator<(const Vertex& other) const {
        return x < other.x || (x == other.x && y < other.y);
    }

    bool operator==(const Vertex& other) const {
        return x == other.x && y == other.y;
    }
};

bool pointInRing(const Point& point, const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return false;
    }
    bool inside = false;
    for (std::size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const Point& a = ring[i];
        const Point& b = ring[j];
        const bool crosses = ((a.y > point.y) != (b.y > point.y)) &&
            (point.x < (b.x - a.x) * (point.y - a.y) /
                ((b.y - a.y) + 1e-30) + a.x);
        if (crosses) {
            inside = !inside;
        }
    }
    return inside;
}

bool pointInPolygon(const Point& point, const Polygon& polygon) {
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

bool checkedProduct(
    std::size_t first,
    std::size_t second,
    std::size_t& product) {
    if (first != 0 &&
        second > std::numeric_limits<std::size_t>::max() / first) {
        return false;
    }
    product = first * second;
    return true;
}

struct RasterBounds {
    int minX = 0;
    int minY = 0;
    int width = 0;
    int height = 0;
    std::size_t cellCount = 0;
};

bool computeRasterBounds(
    const Grid2D& grid,
    const Polygon& polygon,
    RasterBounds& bounds) {
    if (polygon.points.size() < 3) {
        return false;
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

    bounds.minX = std::max(0, static_cast<int>(std::floor(
        (minWorldX - grid.sx()) / grid.dx())) - 1);
    bounds.minY = std::max(0, static_cast<int>(std::floor(
        (minWorldY - grid.sy()) / grid.dy())) - 1);
    const int maxX = std::min(grid.nx() - 1, static_cast<int>(std::ceil(
        (maxWorldX - grid.sx()) / grid.dx())));
    const int maxY = std::min(grid.ny() - 1, static_cast<int>(std::ceil(
        (maxWorldY - grid.sy()) / grid.dy())));
    bounds.width = std::max(0, maxX - bounds.minX + 1);
    bounds.height = std::max(0, maxY - bounds.minY + 1);
    return checkedProduct(
        static_cast<std::size_t>(bounds.width),
        static_cast<std::size_t>(bounds.height),
        bounds.cellCount);
}

bool ringCoordinatesSafeForGrid(
    const Grid2D& grid,
    const std::vector<Point>& ring) {
    if (ring.size() < 3) {
        return false;
    }
    constexpr double marginCells = 2.0;
    for (const Point& point : ring) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
            return false;
        }
        const double gridX = (point.x - grid.sx()) / grid.dx();
        const double gridY = (point.y - grid.sy()) / grid.dy();
        if (!std::isfinite(gridX) || !std::isfinite(gridY) ||
            gridX < -marginCells ||
            gridX > static_cast<double>(grid.nx()) + marginCells ||
            gridY < -marginCells ||
            gridY > static_cast<double>(grid.ny()) + marginCells) {
            return false;
        }
    }
    return true;
}

bool polygonCoordinatesSafeForGrid(
    const Grid2D& grid,
    const Polygon& polygon) {
    if (!ringCoordinatesSafeForGrid(grid, polygon.points)) {
        return false;
    }
    for (const std::vector<Point>& hole : polygon.holes) {
        if (!ringCoordinatesSafeForGrid(grid, hole)) {
            return false;
        }
    }
    return true;
}

bool rasterizePolygon(
    const Grid2D& grid,
    const Polygon& polygon,
    const RasterBounds& bounds,
    LocalMask& mask) {
    mask.minX = bounds.minX;
    mask.minY = bounds.minY;
    mask.width = bounds.width;
    mask.height = bounds.height;
    mask.values.assign(bounds.cellCount, 0);

    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            const int gridX = mask.minX + x;
            const int gridY = mask.minY + y;
            const Point center{
                grid.worldX(gridX + 0.5),
                grid.worldY(gridY + 0.5)
            };
            mask.set(x, y, pointInPolygon(center, polygon));
        }
    }
    return true;
}

std::vector<double> sortedOccupiedWidths(const std::vector<double>& widths) {
    std::vector<double> occupied;
    for (double width : widths) {
        if (width > 0.0) {
            occupied.push_back(width);
        }
    }
    std::sort(occupied.begin(), occupied.end());
    return occupied;
}

double median(std::vector<double> values) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    if ((values.size() % 2) == 0) {
        return 0.5 * (values[middle - 1] + values[middle]);
    }
    return values[middle];
}

std::vector<ProjectedCell> projectMaskCells(
    const LocalMask& mask,
    double& minAlong,
    int& binCount) {
    std::vector<std::pair<double, double>> points;
    points.reserve(static_cast<std::size_t>(mask.count()));
    double meanX = 0.0;
    double meanY = 0.0;
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            const double gx = static_cast<double>(mask.minX + x) + 0.5;
            const double gy = static_cast<double>(mask.minY + y) + 0.5;
            points.push_back({gx, gy});
            meanX += gx;
            meanY += gy;
        }
    }
    if (points.empty()) {
        minAlong = 0.0;
        binCount = 0;
        return {};
    }
    meanX /= static_cast<double>(points.size());
    meanY /= static_cast<double>(points.size());

    double covXX = 0.0;
    double covXY = 0.0;
    double covYY = 0.0;
    for (const auto& point : points) {
        const double dx = point.first - meanX;
        const double dy = point.second - meanY;
        covXX += dx * dx;
        covXY += dx * dy;
        covYY += dy * dy;
    }
    const double angle = 0.5 * std::atan2(2.0 * covXY, covXX - covYY);
    const double axisX = std::cos(angle);
    const double axisY = std::sin(angle);
    const double normalX = -axisY;
    const double normalY = axisX;

    minAlong = std::numeric_limits<double>::infinity();
    double maxAlong = -std::numeric_limits<double>::infinity();
    std::vector<double> alongValues;
    alongValues.reserve(points.size());
    for (const auto& point : points) {
        const double dx = point.first - meanX;
        const double dy = point.second - meanY;
        const double along = dx * axisX + dy * axisY;
        alongValues.push_back(along);
        minAlong = std::min(minAlong, along);
        maxAlong = std::max(maxAlong, along);
    }
    binCount = std::max(1, static_cast<int>(std::floor(maxAlong - minAlong)) + 1);

    std::vector<ProjectedCell> projected;
    projected.reserve(points.size());
    std::size_t pointIndex = 0;
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            const double gx = points[pointIndex].first;
            const double gy = points[pointIndex].second;
            const double dx = gx - meanX;
            const double dy = gy - meanY;
            const double along = alongValues[pointIndex];
            const double across = dx * normalX + dy * normalY;
            const int bin = std::clamp(
                static_cast<int>(std::floor(along - minAlong)),
                0,
                binCount - 1);
            projected.push_back({x, y, bin, across});
            ++pointIndex;
        }
    }
    return projected;
}

CandidateWindow makeWindowFromBins(
    const LocalMask& mask,
    const std::vector<ProjectedCell>& projected,
    int firstBin,
    int lastBin,
    int padding,
    double baselineWidth,
    double localWidth,
    int occupiedBinCount) {
    CandidateWindow window;
    window.minX = mask.width;
    window.minY = mask.height;
    window.maxX = -1;
    window.maxY = -1;
    for (const ProjectedCell& cell : projected) {
        if (cell.bin < firstBin || cell.bin > lastBin) {
            continue;
        }
        window.minX = std::min(window.minX, cell.localX);
        window.maxX = std::max(window.maxX, cell.localX);
        window.minY = std::min(window.minY, cell.localY);
        window.maxY = std::max(window.maxY, cell.localY);
        ++window.candidateCells;
    }
    window.minX = std::max(0, window.minX - padding);
    window.maxX = std::min(mask.width - 1, window.maxX + padding);
    window.minY = std::max(0, window.minY - padding);
    window.maxY = std::min(mask.height - 1, window.maxY + padding);
    window.baselineWidth = baselineWidth;
    window.localWidth = localWidth;
    window.widthRatio = localWidth / std::max(1e-9, baselineWidth);
    window.spanRatio = static_cast<double>(lastBin - firstBin + 1) /
        static_cast<double>(std::max(1, occupiedBinCount));
    return window;
}

std::vector<CandidateWindow> detectWidthCandidates(
    const LocalMask& mask,
    const LocalPolygonQualityOptions& options) {
    double minAlong = 0.0;
    int binCount = 0;
    const std::vector<ProjectedCell> projected = projectMaskCells(mask, minAlong, binCount);
    if (projected.size() < static_cast<std::size_t>(std::max(8, options.minExtraCells)) ||
        binCount < 3) {
        return {};
    }

    std::vector<double> minAcross(static_cast<std::size_t>(binCount),
                                  std::numeric_limits<double>::infinity());
    std::vector<double> maxAcross(static_cast<std::size_t>(binCount),
                                  -std::numeric_limits<double>::infinity());
    std::vector<int> counts(static_cast<std::size_t>(binCount), 0);
    for (const ProjectedCell& cell : projected) {
        minAcross[static_cast<std::size_t>(cell.bin)] =
            std::min(minAcross[static_cast<std::size_t>(cell.bin)], cell.across);
        maxAcross[static_cast<std::size_t>(cell.bin)] =
            std::max(maxAcross[static_cast<std::size_t>(cell.bin)], cell.across);
        ++counts[static_cast<std::size_t>(cell.bin)];
    }

    std::vector<double> widths(static_cast<std::size_t>(binCount), 0.0);
    int occupiedBins = 0;
    for (int bin = 0; bin < binCount; ++bin) {
        if (counts[static_cast<std::size_t>(bin)] <= 0) {
            continue;
        }
        widths[static_cast<std::size_t>(bin)] =
            maxAcross[static_cast<std::size_t>(bin)] -
            minAcross[static_cast<std::size_t>(bin)] + 1.0;
        ++occupiedBins;
    }
    std::vector<double> lowerPool = sortedOccupiedWidths(widths);
    if (lowerPool.empty()) {
        return {};
    }
    lowerPool.resize(std::max<std::size_t>(
        1,
        static_cast<std::size_t>(std::ceil(lowerPool.size() * 0.65))));
    const double baselineWidth = options.referenceWidthCells > 0.0
        ? options.referenceWidthCells
        : std::max(1.0, median(lowerPool));

    std::vector<CandidateWindow> windows;
    int bin = 0;
    while (bin < binCount) {
        const bool suspect = counts[static_cast<std::size_t>(bin)] > 0 &&
            widths[static_cast<std::size_t>(bin)] >= baselineWidth * options.widthRatio;
        if (!suspect) {
            ++bin;
            continue;
        }
        const int firstBin = bin;
        double localWidth = widths[static_cast<std::size_t>(bin)];
        int actualCells = 0;
        while (bin < binCount && counts[static_cast<std::size_t>(bin)] > 0 &&
               widths[static_cast<std::size_t>(bin)] >= baselineWidth * options.widthRatio) {
            localWidth = std::max(localWidth, widths[static_cast<std::size_t>(bin)]);
            actualCells += counts[static_cast<std::size_t>(bin)];
            ++bin;
        }
        const int lastBin = bin - 1;
        const int extraCells = static_cast<int>(std::lround(
            static_cast<double>(actualCells) -
            baselineWidth * static_cast<double>(lastBin - firstBin + 1)));
        if (extraCells < options.minExtraCells) {
            continue;
        }

        CandidateWindow window = makeWindowFromBins(
            mask,
            projected,
            firstBin,
            lastBin,
            options.candidatePaddingCells,
            baselineWidth,
            localWidth,
            occupiedBins);
        const double edgeDistanceRatio = static_cast<double>(std::min(
            firstBin,
            std::max(0, binCount - 1 - lastBin))) /
            static_cast<double>(std::max(1, binCount));
        if (window.spanRatio >= options.maxLocalizedSpanRatio) {
            window.type = LocalQualityCandidateType::LegalWideBelt;
        } else if (edgeDistanceRatio <= options.endpointGuardRatio) {
            window.type = LocalQualityCandidateType::NaturalEndCap;
        } else {
            window.type = LocalQualityCandidateType::LocalBulge;
        }
        windows.push_back(window);
    }
    std::sort(windows.begin(), windows.end(), [](const CandidateWindow& left, const CandidateWindow& right) {
        return left.candidateCells > right.candidateCells;
    });
    return windows;
}

std::vector<CandidateWindow> detectBridgeCandidates(
    const Grid2D& grid,
    const LocalMask& mask,
    double supportThreshold,
    const LocalPolygonQualityOptions& options) {
    double minAlong = 0.0;
    int binCount = 0;
    const std::vector<ProjectedCell> projected = projectMaskCells(mask, minAlong, binCount);
    if (projected.empty() || binCount < 3) {
        return {};
    }
    std::vector<int> counts(static_cast<std::size_t>(binCount), 0);
    std::vector<int> supported(static_cast<std::size_t>(binCount), 0);
    for (const ProjectedCell& cell : projected) {
        ++counts[static_cast<std::size_t>(cell.bin)];
        const double value = grid.value(mask.minX + cell.localX, mask.minY + cell.localY);
        if (std::isfinite(value) && value >= supportThreshold) {
            ++supported[static_cast<std::size_t>(cell.bin)];
        }
    }
    int occupiedBins = 0;
    for (int count : counts) {
        occupiedBins += count > 0 ? 1 : 0;
    }

    std::vector<CandidateWindow> windows;
    int bin = 1;
    while (bin + 1 < binCount) {
        const double ratio = counts[static_cast<std::size_t>(bin)] > 0
            ? static_cast<double>(supported[static_cast<std::size_t>(bin)]) /
                static_cast<double>(counts[static_cast<std::size_t>(bin)])
            : 1.0;
        if (counts[static_cast<std::size_t>(bin)] <= 0 ||
            ratio > options.bridgeMaxSupportRatio) {
            ++bin;
            continue;
        }
        const int firstBin = bin;
        int lowCells = 0;
        double supportSum = 0.0;
        while (bin + 1 < binCount && counts[static_cast<std::size_t>(bin)] > 0) {
            const double currentRatio =
                static_cast<double>(supported[static_cast<std::size_t>(bin)]) /
                static_cast<double>(counts[static_cast<std::size_t>(bin)]);
            if (currentRatio > options.bridgeMaxSupportRatio) {
                break;
            }
            lowCells += counts[static_cast<std::size_t>(bin)];
            supportSum += currentRatio;
            ++bin;
        }
        const int lastBin = bin - 1;
        if (lowCells < options.bridgeMinCells || firstBin <= 0 || lastBin + 1 >= binCount ||
            supported[static_cast<std::size_t>(firstBin - 1)] <= 0 ||
            supported[static_cast<std::size_t>(lastBin + 1)] <= 0) {
            continue;
        }
        CandidateWindow window = makeWindowFromBins(
            mask,
            projected,
            firstBin,
            lastBin,
            options.candidatePaddingCells,
            0.0,
            0.0,
            occupiedBins);
        window.type = LocalQualityCandidateType::LowSupportBridge;
        window.supportRatio = supportSum /
            static_cast<double>(std::max(1, lastBin - firstBin + 1));
        windows.push_back(window);
    }
    return windows;
}

double windowOverlapRatio(
    const CandidateWindow& left,
    const CandidateWindow& right) {
    const int minX = std::max(left.minX, right.minX);
    const int maxX = std::min(left.maxX, right.maxX);
    const int minY = std::max(left.minY, right.minY);
    const int maxY = std::min(left.maxY, right.maxY);
    if (minX > maxX || minY > maxY) {
        return 0.0;
    }
    const int intersection = (maxX - minX + 1) * (maxY - minY + 1);
    const int leftArea = (left.maxX - left.minX + 1) *
        (left.maxY - left.minY + 1);
    const int rightArea = (right.maxX - right.minX + 1) *
        (right.maxY - right.minY + 1);
    return static_cast<double>(intersection) /
        static_cast<double>(std::max(1, std::min(leftArea, rightArea)));
}

void appendUniqueCandidate(
    std::vector<CandidateWindow>& windows,
    const CandidateWindow& candidate) {
    for (CandidateWindow& existing : windows) {
        if (existing.type != candidate.type ||
            windowOverlapRatio(existing, candidate) < 0.80) {
            continue;
        }
        if (candidate.candidateCells > existing.candidateCells) {
            existing = candidate;
        }
        return;
    }
    windows.push_back(candidate);
}

LocalMask supportConstrainedProposal(
    const Grid2D& grid,
    const LocalMask& original,
    const CandidateWindow& window,
    double supportThreshold,
    int supportBufferCells,
    int& supportCellCount) {
    LocalMask support = original;
    std::fill(support.values.begin(), support.values.end(), 0);
    supportCellCount = 0;
    for (int y = window.minY; y <= window.maxY; ++y) {
        for (int x = window.minX; x <= window.maxX; ++x) {
            if (!original.get(x, y)) {
                continue;
            }
            const double value = grid.value(original.minX + x, original.minY + y);
            if (std::isfinite(value) && value >= supportThreshold) {
                support.set(x, y, true);
                ++supportCellCount;
            }
        }
    }

    LocalMask buffered = support;
    for (int y = window.minY; y <= window.maxY; ++y) {
        for (int x = window.minX; x <= window.maxX; ++x) {
            bool active = false;
            for (int yy = y - supportBufferCells; yy <= y + supportBufferCells && !active; ++yy) {
                for (int xx = x - supportBufferCells; xx <= x + supportBufferCells; ++xx) {
                    if (support.get(xx, yy)) {
                        active = true;
                        break;
                    }
                }
            }
            buffered.set(x, y, active && original.get(x, y));
        }
    }

    LocalMask proposal = original;
    for (int y = window.minY; y <= window.maxY; ++y) {
        for (int x = window.minX; x <= window.maxX; ++x) {
            proposal.set(x, y, buffered.get(x, y));
        }
    }
    return proposal;
}

int componentCount(const LocalMask& mask) {
    std::vector<unsigned char> visited(mask.values.size(), 0);
    constexpr int offsets[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    int components = 0;
    for (int y0 = 0; y0 < mask.height; ++y0) {
        for (int x0 = 0; x0 < mask.width; ++x0) {
            const std::size_t start = static_cast<std::size_t>(y0 * mask.width + x0);
            if (!mask.get(x0, y0) || visited[start]) {
                continue;
            }
            ++components;
            std::queue<std::pair<int, int>> queue;
            queue.push({x0, y0});
            visited[start] = 1;
            while (!queue.empty()) {
                const auto cell = queue.front();
                queue.pop();
                for (const auto& offset : offsets) {
                    const int x = cell.first + offset[0];
                    const int y = cell.second + offset[1];
                    if (!mask.get(x, y)) {
                        continue;
                    }
                    const std::size_t pos = static_cast<std::size_t>(y * mask.width + x);
                    if (!visited[pos]) {
                        visited[pos] = 1;
                        queue.push({x, y});
                    }
                }
            }
        }
    }
    return components;
}

std::vector<Point> removeCollinear(const std::vector<Point>& points) {
    if (points.size() < 3) {
        return points;
    }
    std::vector<Point> compact;
    compact.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point& previous = points[(i + points.size() - 1) % points.size()];
        const Point& current = points[i];
        const Point& next = points[(i + 1) % points.size()];
        const double cross = (current.x - previous.x) * (next.y - current.y) -
            (current.y - previous.y) * (next.x - current.x);
        if (std::abs(cross) > 1e-12) {
            compact.push_back(current);
        }
    }
    return compact.size() >= 3 ? compact : points;
}

std::vector<std::vector<Point>> traceBoundaryLoops(const Grid2D& grid, const LocalMask& mask) {
    std::map<Vertex, std::vector<Vertex>> nextByStart;
    auto addEdge = [&nextByStart](Vertex from, Vertex to) {
        nextByStart[from].push_back(to);
    };
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            const int gx = mask.minX + x;
            const int gy = mask.minY + y;
            if (!mask.get(x, y - 1)) {
                addEdge({gx, gy}, {gx + 1, gy});
            }
            if (!mask.get(x + 1, y)) {
                addEdge({gx + 1, gy}, {gx + 1, gy + 1});
            }
            if (!mask.get(x, y + 1)) {
                addEdge({gx + 1, gy + 1}, {gx, gy + 1});
            }
            if (!mask.get(x - 1, y)) {
                addEdge({gx, gy + 1}, {gx, gy});
            }
        }
    }
    for (auto& item : nextByStart) {
        std::sort(item.second.begin(), item.second.end());
    }

    std::vector<std::vector<Point>> loops;
    const int maxSteps = static_cast<int>(nextByStart.size() * 8 + 8);
    while (!nextByStart.empty()) {
        const Vertex start = nextByStart.begin()->first;
        Vertex current = start;
        std::vector<Vertex> vertices;
        vertices.push_back(current);
        int guard = 0;
        while (guard++ < maxSteps) {
            auto found = nextByStart.find(current);
            if (found == nextByStart.end() || found->second.empty()) {
                break;
            }
            const Vertex next = found->second.back();
            found->second.pop_back();
            if (found->second.empty()) {
                nextByStart.erase(found);
            }
            current = next;
            if (current == start) {
                break;
            }
            vertices.push_back(current);
        }
        if (!(current == start) || vertices.size() < 3) {
            continue;
        }
        std::vector<Point> loop;
        loop.reserve(vertices.size());
        for (const Vertex& vertex : vertices) {
            loop.push_back({grid.worldX(vertex.x), grid.worldY(vertex.y)});
        }
        loop = removeCollinear(loop);
        if (loop.size() >= 3) {
            loops.push_back(std::move(loop));
        }
    }
    return loops;
}

double signedRingArea(const std::vector<Point>& ring) {
    double area = 0.0;
    for (std::size_t index = 0; index < ring.size(); ++index) {
        const Point& first = ring[index];
        const Point& second = ring[(index + 1) % ring.size()];
        area += first.x * second.y - second.x * first.y;
    }
    return area * 0.5;
}

bool exactPointEqual(const Point& left, const Point& right) {
    return left.x == right.x && left.y == right.y;
}

std::vector<Point> forwardRingPath(
    const std::vector<Point>& ring,
    std::size_t first,
    std::size_t last) {
    std::vector<Point> path;
    if (ring.empty()) {
        return path;
    }
    std::size_t index = first;
    path.push_back(ring[index]);
    while (index != last) {
        index = (index + 1) % ring.size();
        path.push_back(ring[index]);
        if (path.size() > ring.size() + 1) {
            return {};
        }
    }
    return path;
}

bool pointInsideClosedWindow(
    const Point& point,
    double minX,
    double maxX,
    double minY,
    double maxY) {
    constexpr double epsilon = 1e-9;
    return point.x >= minX - epsilon && point.x <= maxX + epsilon &&
        point.y >= minY - epsilon && point.y <= maxY + epsilon;
}

bool pointStrictlyInsideWindow(
    const Point& point,
    double minX,
    double maxX,
    double minY,
    double maxY) {
    constexpr double epsilon = 1e-9;
    return point.x > minX + epsilon && point.x < maxX - epsilon &&
        point.y > minY + epsilon && point.y < maxY - epsilon;
}

bool localPathWithinWindow(
    const std::vector<Point>& path,
    double minX,
    double maxX,
    double minY,
    double maxY) {
    if (path.size() < 2) {
        return false;
    }
    return std::all_of(path.begin(), path.end(), [&](const Point& point) {
        return pointInsideClosedWindow(point, minX, maxX, minY, maxY);
    });
}

bool outsidePathAvoidsWindowInterior(
    const std::vector<Point>& path,
    double minX,
    double maxX,
    double minY,
    double maxY) {
    if (path.size() < 2) {
        return false;
    }
    for (std::size_t index = 1; index + 1 < path.size(); ++index) {
        if (pointStrictlyInsideWindow(
                path[index], minX, maxX, minY, maxY)) {
            return false;
        }
    }
    return true;
}

bool pointOnSegmentExactGeometry(
    const Point& point,
    const Point& first,
    const Point& second) {
    constexpr double epsilon = 1e-9;
    const double cross = (second.x - first.x) * (point.y - first.y) -
        (second.y - first.y) * (point.x - first.x);
    if (std::abs(cross) > epsilon) {
        return false;
    }
    return point.x >= std::min(first.x, second.x) - epsilon &&
        point.x <= std::max(first.x, second.x) + epsilon &&
        point.y >= std::min(first.y, second.y) - epsilon &&
        point.y <= std::max(first.y, second.y) + epsilon;
}

bool pointOnPathGeometry(const Point& point, const std::vector<Point>& path) {
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        if (pointOnSegmentExactGeometry(point, path[index], path[index + 1])) {
            return true;
        }
    }
    return false;
}

bool pathsHaveSameGeometry(
    const std::vector<Point>& first,
    const std::vector<Point>& second) {
    if (first.size() < 2 || second.size() < 2 ||
        !exactPointEqual(first.front(), second.front()) ||
        !exactPointEqual(first.back(), second.back())) {
        return false;
    }
    for (const Point& point : first) {
        if (!pointOnPathGeometry(point, second)) {
            return false;
        }
    }
    for (const Point& point : second) {
        if (!pointOnPathGeometry(point, first)) {
            return false;
        }
    }
    return true;
}

std::vector<Point> spliceUniqueLocalRingArc(
    const Grid2D& grid,
    const LocalMask& mask,
    const CandidateWindow& window,
    const std::vector<Point>& originalRing,
    std::vector<Point> replacementRing) {
    if (originalRing.size() < 3 || replacementRing.size() < 3) {
        return {};
    }
    if (signedRingArea(originalRing) * signedRingArea(replacementRing) < 0.0) {
        std::reverse(replacementRing.begin(), replacementRing.end());
    }

    const double minWorldX = grid.worldX(mask.minX + window.minX);
    const double maxWorldX = grid.worldX(mask.minX + window.maxX + 1);
    const double minWorldY = grid.worldY(mask.minY + window.minY);
    const double maxWorldY = grid.worldY(mask.minY + window.maxY + 1);

    struct CommonVertex {
        std::size_t originalIndex = 0;
        std::size_t replacementIndex = 0;
    };
    std::vector<CommonVertex> common;
    for (std::size_t originalIndex = 0;
         originalIndex < originalRing.size();
         ++originalIndex) {
        if (!pointInsideClosedWindow(
                originalRing[originalIndex],
                minWorldX,
                maxWorldX,
                minWorldY,
                maxWorldY)) {
            continue;
        }
        for (std::size_t replacementIndex = 0;
             replacementIndex < replacementRing.size();
             ++replacementIndex) {
            if (exactPointEqual(
                    originalRing[originalIndex],
                    replacementRing[replacementIndex])) {
                common.push_back({originalIndex, replacementIndex});
            }
        }
    }

    std::vector<Point> uniqueSplice;
    int validSpliceCount = 0;
    for (std::size_t firstIndex = 0; firstIndex < common.size(); ++firstIndex) {
        for (std::size_t secondIndex = firstIndex + 1;
             secondIndex < common.size();
             ++secondIndex) {
            const CommonVertex& first = common[firstIndex];
            const CommonVertex& second = common[secondIndex];
            const std::vector<Point> originalForward = forwardRingPath(
                originalRing, first.originalIndex, second.originalIndex);
            const std::vector<Point> originalBackward = forwardRingPath(
                originalRing, second.originalIndex, first.originalIndex);
            const bool forwardIsLocal = localPathWithinWindow(
                originalForward,
                minWorldX,
                maxWorldX,
                minWorldY,
                maxWorldY) && outsidePathAvoidsWindowInterior(
                    originalBackward,
                    minWorldX,
                    maxWorldX,
                    minWorldY,
                    maxWorldY);
            const bool backwardIsLocal = localPathWithinWindow(
                originalBackward,
                minWorldX,
                maxWorldX,
                minWorldY,
                maxWorldY) && outsidePathAvoidsWindowInterior(
                    originalForward,
                    minWorldX,
                    maxWorldX,
                    minWorldY,
                    maxWorldY);
            if (forwardIsLocal == backwardIsLocal) {
                continue;
            }

            const CommonVertex& localStart = forwardIsLocal ? first : second;
            const CommonVertex& localEnd = forwardIsLocal ? second : first;
            const std::vector<Point>& originalOutside = forwardIsLocal
                ? originalBackward
                : originalForward;
            const std::vector<Point> replacementLocal = forwardRingPath(
                replacementRing,
                localStart.replacementIndex,
                localEnd.replacementIndex);
            const std::vector<Point> replacementOutside = forwardRingPath(
                replacementRing,
                localEnd.replacementIndex,
                localStart.replacementIndex);
            if (!localPathWithinWindow(
                    replacementLocal,
                    minWorldX,
                    maxWorldX,
                    minWorldY,
                    maxWorldY) ||
                !outsidePathAvoidsWindowInterior(
                    replacementOutside,
                    minWorldX,
                    maxWorldX,
                    minWorldY,
                    maxWorldY) ||
                !pathsHaveSameGeometry(originalOutside, replacementOutside)) {
                continue;
            }

            std::vector<Point> splice = replacementLocal;
            for (std::size_t index = 1;
                 index + 1 < originalOutside.size();
                 ++index) {
                splice.push_back(originalOutside[index]);
            }
            if (splice.size() < 3) {
                continue;
            }
            ++validSpliceCount;
            uniqueSplice = std::move(splice);
        }
    }
    return validSpliceCount == 1 ? uniqueSplice : std::vector<Point>{};
}

double coverageLossRatio(
    const Grid2D& grid,
    const LocalMask& original,
    const LocalMask& proposal,
    double threshold) {
    int before = 0;
    int after = 0;
    for (int y = 0; y < original.height; ++y) {
        for (int x = 0; x < original.width; ++x) {
            if (!original.get(x, y)) {
                continue;
            }
            const double value = grid.value(original.minX + x, original.minY + y);
            if (!std::isfinite(value) || value < threshold) {
                continue;
            }
            ++before;
            if (proposal.get(x, y)) {
                ++after;
            }
        }
    }
    return before > 0
        ? static_cast<double>(before - after) / static_cast<double>(before)
        : 0.0;
}

double backgroundLeakageRate(
    const Grid2D& grid,
    const LocalMask& mask,
    double weakThreshold) {
    int polygonCells = 0;
    int backgroundCells = 0;
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            ++polygonCells;
            const double value = grid.value(mask.minX + x, mask.minY + y);
            if (!std::isfinite(value) || value < weakThreshold) {
                ++backgroundCells;
            }
        }
    }
    return polygonCells > 0
        ? static_cast<double>(backgroundCells) / static_cast<double>(polygonCells)
        : 0.0;
}

double maskJaccard(const LocalMask& left, const LocalMask& right) {
    int intersection = 0;
    int unionCount = 0;
    for (std::size_t i = 0; i < left.values.size(); ++i) {
        const bool a = left.values[i] != 0;
        const bool b = right.values[i] != 0;
        intersection += a && b ? 1 : 0;
        unionCount += a || b ? 1 : 0;
    }
    return unionCount > 0
        ? static_cast<double>(intersection) / static_cast<double>(unionCount)
        : 1.0;
}

void computePolygonStats(const Grid2D& grid, const LocalMask& mask, Polygon& polygon) {
    double sum = 0.0;
    double maxValue = -std::numeric_limits<double>::infinity();
    int cells = 0;
    for (int y = 0; y < mask.height; ++y) {
        for (int x = 0; x < mask.width; ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            const double value = grid.value(mask.minX + x, mask.minY + y);
            if (std::isfinite(value)) {
                sum += value;
                maxValue = std::max(maxValue, value);
            }
            ++cells;
        }
    }
    polygon.area = polygonArea(polygon.points);
    polygon.perimeter = polygonPerimeter(polygon.points);
    polygon.cellCount = cells;
    polygon.meanValue = cells > 0 ? sum / static_cast<double>(cells) : 0.0;
    polygon.maxValue = std::isfinite(maxValue) ? maxValue : 0.0;
}

void countRejection(LocalPolygonQualityStats& stats, LocalQualityRejectionReason reason) {
    ++stats.rejectedCount;
    switch (reason) {
    case LocalQualityRejectionReason::InsufficientEvidence:
        ++stats.insufficientEvidenceRejectedCount;
        break;
    case LocalQualityRejectionReason::NoMeaningfulChange:
        ++stats.noMeaningfulChangeRejectedCount;
        break;
    case LocalQualityRejectionReason::ExcessiveAreaLoss:
        ++stats.areaLossRejectedCount;
        break;
    case LocalQualityRejectionReason::StrongCoverageLoss:
        ++stats.strongCoverageRejectedCount;
        break;
    case LocalQualityRejectionReason::WeakCoverageLoss:
        ++stats.weakCoverageRejectedCount;
        break;
    case LocalQualityRejectionReason::BackgroundLeakage:
        ++stats.backgroundLeakageRejectedCount;
        break;
    case LocalQualityRejectionReason::TopologyRisk:
        ++stats.topologyRejectedCount;
        break;
    case LocalQualityRejectionReason::StabilityRisk:
        ++stats.stabilityRejectedCount;
        break;
    case LocalQualityRejectionReason::InvalidGeometry:
        ++stats.invalidGeometryRejectedCount;
        break;
    case LocalQualityRejectionReason::None:
        break;
    }
}

void rejectEvidence(
    LocalPolygonQualityEvidence& evidence,
    LocalPolygonQualityStats& stats,
    LocalQualityRejectionReason reason) {
    evidence.decision = LocalQualityDecision::Rejected;
    evidence.rejectionReason = reason;
    countRejection(stats, reason);
}

void recordEvidence(
    LocalPolygonQualityResult& result,
    const LocalPolygonQualityOptions& options,
    const LocalPolygonQualityEvidence& evidence) {
    ++result.stats.totalEvidence;
    if (result.evidence.size() < options.maxEvidenceRecords) {
        result.evidence.push_back(evidence);
        ++result.stats.storedEvidence;
    } else {
        ++result.stats.truncatedEvidence;
    }
}

void validateOptions(double effectiveThreshold, const LocalPolygonQualityOptions& options) {
    const double values[] = {
        effectiveThreshold,
        options.widthRatio,
        options.referenceWidthCells,
        options.supportThresholdRatio,
        options.bridgeMaxSupportRatio,
        options.maxAreaLossRatio,
        options.maxStrongCoverageLossRatio,
        options.maxWeakCoverageLossRatio,
        options.maxBackgroundLeakageIncreaseRatio,
        options.maxLocalizedSpanRatio,
        options.endpointGuardRatio,
        options.stabilityThresholdDeltaRatio,
        options.minStabilityJaccard
    };
    for (double value : values) {
        if (!std::isfinite(value)) {
            throw std::runtime_error("Local polygon quality options must be finite.");
        }
    }
    if (options.widthRatio <= 1.0 || options.widthRatio > 20.0) {
        throw std::runtime_error("local quality widthRatio must be in (1, 20].");
    }
    if (options.referenceWidthCells < 0.0 || options.referenceWidthCells > 1000.0) {
        throw std::runtime_error("local quality referenceWidthCells is out of range.");
    }
    if (options.minExtraCells < 1 || options.candidatePaddingCells < 0 ||
        options.supportBufferCells < 0 || options.minSupportCells < 1 ||
        options.bridgeMinCells < 1) {
        throw std::runtime_error("Local polygon quality cell counts must be valid.");
    }
    const auto requireUnit = [](double value, const char* name) {
        if (value < 0.0 || value > 1.0) {
            throw std::runtime_error(std::string(name) + " must be between 0 and 1.");
        }
    };
    if (options.supportThresholdRatio < 0.0 ||
        options.supportThresholdRatio > 2.0) {
        throw std::runtime_error(
            "supportThresholdRatio must be between 0 and 2.");
    }
    requireUnit(options.bridgeMaxSupportRatio, "bridgeMaxSupportRatio");
    requireUnit(options.maxAreaLossRatio, "maxAreaLossRatio");
    requireUnit(options.maxStrongCoverageLossRatio, "maxStrongCoverageLossRatio");
    requireUnit(options.maxWeakCoverageLossRatio, "maxWeakCoverageLossRatio");
    requireUnit(options.maxBackgroundLeakageIncreaseRatio, "maxBackgroundLeakageIncreaseRatio");
    requireUnit(options.maxLocalizedSpanRatio, "maxLocalizedSpanRatio");
    requireUnit(options.endpointGuardRatio, "endpointGuardRatio");
    requireUnit(options.stabilityThresholdDeltaRatio, "stabilityThresholdDeltaRatio");
    requireUnit(options.minStabilityJaccard, "minStabilityJaccard");
    if (options.maxCandidatesPerObject == 0 ||
        options.maxTotalCandidates == 0 ||
        options.maxWindowCells == 0 ||
        options.maxObjectCells == 0) {
        throw std::runtime_error(
            "Local polygon quality processing budgets must be positive.");
    }
}

} // namespace

void validateLocalPolygonQualityOptions(
    const LocalPolygonQualityOptions& options) {
    validateOptions(0.0, options);
}

LocalPolygonQualityResult applyLocalPolygonQuality(
    const Grid2D& grid,
    const std::vector<Polygon>& polygons,
    double effectiveThreshold,
    const LocalPolygonQualityOptions& options) {
    validateOptions(effectiveThreshold, options);

    LocalPolygonQualityResult result;
    result.polygons = polygons;
    const double strongThreshold = std::max(0.0, effectiveThreshold);
    const double supportThreshold =
        strongThreshold * options.supportThresholdRatio;
    const double weakCoverageThreshold = strongThreshold * 0.70;
    std::size_t processedCandidateCount = 0;

    for (std::size_t polygonIndex = 0; polygonIndex < result.polygons.size(); ++polygonIndex) {
        Polygon& polygon = result.polygons[polygonIndex];
        if (!polygon.kept || polygon.points.size() < 3) {
            continue;
        }
        if (!polygonCoordinatesSafeForGrid(grid, polygon)) {
            LocalPolygonQualityEvidence evidence;
            evidence.objectIndex = static_cast<int>(polygonIndex + 1);
            evidence.polygonId = polygon.id;
            evidence.type = LocalQualityCandidateType::InvalidGeometry;
            ++result.stats.detectedCount;
            rejectEvidence(
                evidence,
                result.stats,
                LocalQualityRejectionReason::InvalidGeometry);
            recordEvidence(result, options, evidence);
            continue;
        }
        RasterBounds rasterBounds;
        if (!computeRasterBounds(grid, polygon, rasterBounds)) {
            LocalPolygonQualityEvidence evidence;
            evidence.objectIndex = static_cast<int>(polygonIndex + 1);
            evidence.polygonId = polygon.id;
            evidence.type = LocalQualityCandidateType::InvalidGeometry;
            ++result.stats.detectedCount;
            rejectEvidence(
                evidence,
                result.stats,
                LocalQualityRejectionReason::InvalidGeometry);
            recordEvidence(result, options, evidence);
            continue;
        }
        if (rasterBounds.cellCount > options.maxObjectCells) {
            LocalPolygonQualityEvidence evidence;
            evidence.objectIndex = static_cast<int>(polygonIndex + 1);
            evidence.polygonId = polygon.id;
            evidence.type = LocalQualityCandidateType::ObjectBudgetExceeded;
            evidence.candidateCellCount = static_cast<int>(std::min<std::size_t>(
                rasterBounds.cellCount,
                static_cast<std::size_t>(std::numeric_limits<int>::max())));
            evidence.decision = LocalQualityDecision::Review;
            ++result.stats.detectedCount;
            ++result.stats.reviewCount;
            ++result.stats.objectBudgetReviewCount;
            recordEvidence(result, options, evidence);
            continue;
        }
        LocalMask original;
        if (!rasterizePolygon(grid, polygon, rasterBounds, original)) {
            LocalPolygonQualityEvidence evidence;
            evidence.objectIndex = static_cast<int>(polygonIndex + 1);
            evidence.polygonId = polygon.id;
            evidence.type = LocalQualityCandidateType::InvalidGeometry;
            ++result.stats.detectedCount;
            rejectEvidence(
                evidence,
                result.stats,
                LocalQualityRejectionReason::InvalidGeometry);
            recordEvidence(result, options, evidence);
            continue;
        }
        const std::size_t objectCellCount =
            static_cast<std::size_t>(original.count());
        if (objectCellCount > options.maxObjectCells) {
            LocalPolygonQualityEvidence evidence;
            evidence.objectIndex = static_cast<int>(polygonIndex + 1);
            evidence.polygonId = polygon.id;
            evidence.type = LocalQualityCandidateType::ObjectBudgetExceeded;
            evidence.candidateCellCount = static_cast<int>(std::min<std::size_t>(
                objectCellCount,
                static_cast<std::size_t>(std::numeric_limits<int>::max())));
            evidence.decision = LocalQualityDecision::Review;
            ++result.stats.detectedCount;
            ++result.stats.reviewCount;
            ++result.stats.objectBudgetReviewCount;
            recordEvidence(result, options, evidence);
            continue;
        }
        if (objectCellCount < static_cast<std::size_t>(
                std::max(8, options.minExtraCells))) {
            continue;
        }

        std::vector<CandidateWindow> windows = detectWidthCandidates(original, options);
        const std::vector<CandidateWindow> bridgeWindows = detectBridgeCandidates(
                grid,
                original,
                supportThreshold,
                options);
        for (const CandidateWindow& bridge : bridgeWindows) {
            appendUniqueCandidate(windows, bridge);
        }

        std::size_t processedForObject = 0;
        for (const CandidateWindow& window : windows) {
            LocalPolygonQualityEvidence evidence;
            evidence.objectIndex = static_cast<int>(polygonIndex + 1);
            evidence.polygonId = polygon.id;
            evidence.type = window.type;
            evidence.minGridX = original.minX + window.minX;
            evidence.maxGridX = original.minX + window.maxX;
            evidence.minGridY = original.minY + window.minY;
            evidence.maxGridY = original.minY + window.maxY;
            evidence.candidateCellCount = window.candidateCells;
            evidence.baselineWidth = window.baselineWidth;
            evidence.localWidth = window.localWidth;
            evidence.widthRatio = window.widthRatio;
            evidence.spanRatio = window.spanRatio;
            evidence.supportRatio = window.supportRatio;
            ++result.stats.detectedCount;

            if (processedForObject >= options.maxCandidatesPerObject ||
                processedCandidateCount >= options.maxTotalCandidates) {
                evidence.decision = LocalQualityDecision::Review;
                ++result.stats.reviewCount;
                ++result.stats.candidateBudgetReviewCount;
                recordEvidence(result, options, evidence);
                continue;
            }
            ++processedForObject;
            ++processedCandidateCount;

            const std::size_t windowWidth = static_cast<std::size_t>(
                window.maxX - window.minX + 1);
            const std::size_t windowHeight = static_cast<std::size_t>(
                window.maxY - window.minY + 1);
            std::size_t windowCells = 0;
            if (!checkedProduct(windowWidth, windowHeight, windowCells) ||
                windowCells > options.maxWindowCells) {
                evidence.decision = LocalQualityDecision::Review;
                ++result.stats.reviewCount;
                ++result.stats.windowBudgetReviewCount;
                recordEvidence(result, options, evidence);
                continue;
            }

            if (window.type != LocalQualityCandidateType::LocalBulge) {
                evidence.decision = LocalQualityDecision::Review;
                ++result.stats.reviewCount;
                recordEvidence(result, options, evidence);
                continue;
            }

            int supportCellCount = 0;
            LocalMask proposal = supportConstrainedProposal(
                grid,
                original,
                window,
                supportThreshold,
                options.supportBufferCells,
                supportCellCount);
            evidence.supportCellCount = supportCellCount;
            evidence.supportRatio = window.candidateCells > 0
                ? static_cast<double>(supportCellCount) /
                    static_cast<double>(window.candidateCells)
                : 0.0;
            if (supportCellCount < options.minSupportCells) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::InsufficientEvidence);
                recordEvidence(result, options, evidence);
                continue;
            }

            const int originalCells = original.count();
            const int proposalCells = proposal.count();
            evidence.removedCellCount = originalCells - proposalCells;
            if (evidence.removedCellCount <= 0) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::NoMeaningfulChange);
                recordEvidence(result, options, evidence);
                continue;
            }
            evidence.areaLossRatio = static_cast<double>(evidence.removedCellCount) /
                static_cast<double>(std::max(1, originalCells));
            if (evidence.areaLossRatio > options.maxAreaLossRatio) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::ExcessiveAreaLoss);
                recordEvidence(result, options, evidence);
                continue;
            }

            evidence.strongCoverageLossRatio = coverageLossRatio(
                grid,
                original,
                proposal,
                strongThreshold);
            if (evidence.strongCoverageLossRatio > options.maxStrongCoverageLossRatio + 1e-12) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::StrongCoverageLoss);
                recordEvidence(result, options, evidence);
                continue;
            }
            evidence.weakCoverageLossRatio = coverageLossRatio(
                grid,
                original,
                proposal,
                weakCoverageThreshold);
            if (evidence.weakCoverageLossRatio > options.maxWeakCoverageLossRatio + 1e-12) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::WeakCoverageLoss);
                recordEvidence(result, options, evidence);
                continue;
            }
            evidence.backgroundLeakageDelta = backgroundLeakageRate(
                grid,
                proposal,
                weakCoverageThreshold) - backgroundLeakageRate(
                    grid,
                    original,
                    weakCoverageThreshold);
            if (evidence.backgroundLeakageDelta >
                options.maxBackgroundLeakageIncreaseRatio + 1e-12) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::BackgroundLeakage);
                recordEvidence(result, options, evidence);
                continue;
            }

            if (componentCount(proposal) != 1) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::TopologyRisk);
                recordEvidence(result, options, evidence);
                continue;
            }
            std::vector<std::vector<Point>> loops = traceBoundaryLoops(grid, proposal);
            if (loops.size() != 1 || loops.front().size() < 3) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::TopologyRisk);
                recordEvidence(result, options, evidence);
                continue;
            }

            Polygon candidate = polygon;
            candidate.points = spliceUniqueLocalRingArc(
                grid,
                original,
                window,
                polygon.points,
                std::move(loops.front()));
            if (candidate.points.size() < 3) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::TopologyRisk);
                recordEvidence(result, options, evidence);
                continue;
            }
            computePolygonStats(grid, proposal, candidate);
            const double geometryAreaLoss = (polygon.area - candidate.area) /
                std::max(1e-9, polygon.area);
            evidence.areaLossRatio = std::max(evidence.areaLossRatio, geometryAreaLoss);
            if (evidence.areaLossRatio > options.maxAreaLossRatio) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::ExcessiveAreaLoss);
                recordEvidence(result, options, evidence);
                continue;
            }
            if (polygonHasTopologyConflict(candidate, result.polygons, true)) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::TopologyRisk);
                recordEvidence(result, options, evidence);
                continue;
            }

            int ignoredSupportCount = 0;
            const LocalMask lowThresholdProposal = supportConstrainedProposal(
                grid,
                original,
                window,
                supportThreshold * (1.0 - options.stabilityThresholdDeltaRatio),
                options.supportBufferCells,
                ignoredSupportCount);
            const LocalMask highThresholdProposal = supportConstrainedProposal(
                grid,
                original,
                window,
                supportThreshold * (1.0 + options.stabilityThresholdDeltaRatio),
                options.supportBufferCells,
                ignoredSupportCount);
            evidence.stabilityJaccard = maskJaccard(
                lowThresholdProposal,
                highThresholdProposal);
            if (componentCount(lowThresholdProposal) != 1 ||
                componentCount(highThresholdProposal) != 1 ||
                evidence.stabilityJaccard + 1e-12 < options.minStabilityJaccard) {
                rejectEvidence(
                    evidence,
                    result.stats,
                    LocalQualityRejectionReason::StabilityRisk);
                recordEvidence(result, options, evidence);
                continue;
            }

            polygon = std::move(candidate);
            original = std::move(proposal);
            evidence.decision = LocalQualityDecision::Accepted;
            ++result.stats.acceptedCount;
            recordEvidence(result, options, evidence);
        }
    }
    return result;
}

const char* localQualityCandidateTypeName(LocalQualityCandidateType type) {
    switch (type) {
    case LocalQualityCandidateType::LocalBulge:
        return "local_bulge";
    case LocalQualityCandidateType::LowSupportBridge:
        return "low_support_bridge";
    case LocalQualityCandidateType::LegalWideBelt:
        return "legal_wide_belt";
    case LocalQualityCandidateType::NaturalEndCap:
        return "natural_end_cap";
    case LocalQualityCandidateType::ObjectBudgetExceeded:
        return "object_budget_exceeded";
    case LocalQualityCandidateType::InvalidGeometry:
        return "invalid_geometry";
    }
    return "unknown";
}

const char* localQualityDecisionName(LocalQualityDecision decision) {
    switch (decision) {
    case LocalQualityDecision::Accepted:
        return "accepted";
    case LocalQualityDecision::Review:
        return "review";
    case LocalQualityDecision::Rejected:
        return "rejected";
    }
    return "unknown";
}

const char* localQualityRejectionReasonName(LocalQualityRejectionReason reason) {
    switch (reason) {
    case LocalQualityRejectionReason::None:
        return "none";
    case LocalQualityRejectionReason::InsufficientEvidence:
        return "insufficient_evidence";
    case LocalQualityRejectionReason::NoMeaningfulChange:
        return "no_meaningful_change";
    case LocalQualityRejectionReason::ExcessiveAreaLoss:
        return "excessive_area_loss";
    case LocalQualityRejectionReason::StrongCoverageLoss:
        return "strong_coverage_loss";
    case LocalQualityRejectionReason::WeakCoverageLoss:
        return "weak_coverage_loss";
    case LocalQualityRejectionReason::BackgroundLeakage:
        return "background_leakage";
    case LocalQualityRejectionReason::TopologyRisk:
        return "topology_risk";
    case LocalQualityRejectionReason::StabilityRisk:
        return "stability_risk";
    case LocalQualityRejectionReason::InvalidGeometry:
        return "invalid_geometry";
    }
    return "unknown";
}

} // namespace fpt
