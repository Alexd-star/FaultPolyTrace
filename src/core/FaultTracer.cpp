#include "FaultTracer.h"
#include "SkeletonGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fpt {
namespace {

struct Cell {
    int x = 0;
    int y = 0;
};

struct OwnershipCell {
    int x = 0;
    int y = 0;
    int ownerId = 0;
};

struct BranchEnd {
    int branchIndex = -1;
    SkeletonPoint direction;
};

struct BranchGroup {
    int id = 0;
    std::vector<const SkeletonBranch*> branches;
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

class DisjointSet {
public:
    explicit DisjointSet(int size) : parent_(static_cast<std::size_t>(size)), rank_(static_cast<std::size_t>(size), 0) {
        for (int i = 0; i < size; ++i) {
            parent_[static_cast<std::size_t>(i)] = i;
        }
    }

    int find(int value) {
        int& parent = parent_[static_cast<std::size_t>(value)];
        if (parent != value) {
            parent = find(parent);
        }
        return parent;
    }

    void unite(int a, int b) {
        int rootA = find(a);
        int rootB = find(b);
        if (rootA == rootB) {
            return;
        }
        if (rank_[static_cast<std::size_t>(rootA)] < rank_[static_cast<std::size_t>(rootB)]) {
            std::swap(rootA, rootB);
        }
        parent_[static_cast<std::size_t>(rootB)] = rootA;
        if (rank_[static_cast<std::size_t>(rootA)] == rank_[static_cast<std::size_t>(rootB)]) {
            ++rank_[static_cast<std::size_t>(rootA)];
        }
    }

private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

struct GapSupportContext {
    const Grid2D* grid = nullptr;
    double supportThreshold = 0.0;
    double minSupportRatio = 0.0;
};

struct Vertex {
    int x = 0;
    int y = 0;

    bool operator<(const Vertex& other) const {
        if (x == other.x) {
            return y < other.y;
        }
        return x < other.x;
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

bool cellSetContains(const std::vector<unsigned char>& mask, int width, int height, int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    return mask[static_cast<std::size_t>(y * width + x)] != 0;
}

Point toWorldPoint(const Grid2D& grid, const Vertex& vertex) {
    return {grid.worldX(vertex.x), grid.worldY(vertex.y)};
}

Point toWorldPoint(const Grid2D& grid, const SkeletonPoint& point) {
    return {grid.worldX(point.x + 0.5), grid.worldY(point.y + 0.5)};
}

double pointDistance(const Point& a, const Point& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

double vectorNorm(const SkeletonPoint& v) {
    return std::sqrt(static_cast<double>(v.x * v.x + v.y * v.y));
}

double angleBetweenDirections(const SkeletonPoint& a, const SkeletonPoint& b) {
    const double denom = vectorNorm(a) * vectorNorm(b);
    if (denom <= 1e-12) {
        return 0.0;
    }
    double c = static_cast<double>(a.x * b.x + a.y * b.y) / denom;
    c = std::max(-1.0, std::min(1.0, c));
    return std::acos(c) * 180.0 / 3.14159265358979323846;
}

std::string pointKey(const SkeletonPoint& point) {
    return std::to_string(point.x) + "," + std::to_string(point.y);
}

bool gapHasAttributeSupport(const SkeletonPoint& a, const SkeletonPoint& b, void* rawContext) {
    const GapSupportContext* context = static_cast<const GapSupportContext*>(rawContext);
    if (!context || !context->grid || context->minSupportRatio <= 0.0) {
        return true;
    }

    int x0 = a.x;
    int y0 = a.y;
    const int x1 = b.x;
    const int y1 = b.y;
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    int total = 0;
    int supported = 0;
    while (true) {
        if (context->grid->inBounds(x0, y0)) {
            ++total;
            const double value = context->grid->value(x0, y0);
            if (std::isfinite(value) && value >= context->supportThreshold) {
                ++supported;
            }
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }

    if (total <= 2) {
        return true;
    }
    return static_cast<double>(supported) / static_cast<double>(total) >= context->minSupportRatio;
}

std::vector<int> buildGroupOwnershipMap(
    int width,
    int height,
    const std::vector<BranchGroup>& groups,
    int maxDistance) {
    std::vector<int> owner(static_cast<std::size_t>(width * height), -1);
    std::vector<int> distance(static_cast<std::size_t>(width * height), -1);
    if (width <= 0 || height <= 0 || groups.empty() || maxDistance < 0) {
        return owner;
    }

    auto indexOf = [width](int x, int y) {
        return y * width + x;
    };

    std::queue<OwnershipCell> queue;
    for (const BranchGroup& group : groups) {
        for (const SkeletonBranch* branch : group.branches) {
            if (!branch) {
                continue;
            }
            for (const SkeletonPoint& point : branch->points) {
                if (point.x < 0 || point.y < 0 || point.x >= width || point.y >= height) {
                    continue;
                }
                const std::size_t idx = static_cast<std::size_t>(indexOf(point.x, point.y));
                if (distance[idx] == 0 && owner[idx] != group.id) {
                    owner[idx] = -2;
                    continue;
                }
                if (distance[idx] < 0) {
                    owner[idx] = group.id;
                    distance[idx] = 0;
                    queue.push({point.x, point.y, group.id});
                }
            }
        }
    }

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    while (!queue.empty()) {
        const OwnershipCell cell = queue.front();
        queue.pop();
        const int currentDistance = distance[static_cast<std::size_t>(indexOf(cell.x, cell.y))];
        if (currentDistance >= maxDistance) {
            continue;
        }

        for (const auto& offset : offsets) {
            const int nxCell = cell.x + offset[0];
            const int nyCell = cell.y + offset[1];
            if (nxCell < 0 || nyCell < 0 || nxCell >= width || nyCell >= height) {
                continue;
            }
            const std::size_t idx = static_cast<std::size_t>(indexOf(nxCell, nyCell));
            const int nextDistance = currentDistance + 1;
            if (distance[idx] < 0) {
                distance[idx] = nextDistance;
                owner[idx] = cell.ownerId;
                queue.push({nxCell, nyCell, cell.ownerId});
            } else if (distance[idx] == nextDistance && owner[idx] != cell.ownerId) {
                owner[idx] = -2;
            }
        }
    }

    return owner;
}

BinaryMask buildRootAntiMergeCutMask(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const BinaryMask& attributeMask,
    const std::vector<int>& groupOwners,
    int radius,
    double supportRatio,
    double maxBridgeValue) {
    BinaryMask cutMask(grid.nx(), grid.ny());
    if (radius <= 0 ||
        groupOwners.size() != static_cast<std::size_t>(grid.nx() * grid.ny())) {
        return cutMask;
    }

    auto ownerAt = [&grid, &groupOwners](int x, int y) {
        if (!grid.inBounds(x, y)) {
            return -1;
        }
        return groupOwners[static_cast<std::size_t>(y * grid.nx() + x)];
    };

    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (!activeMask.get(x, y) || attributeMask.get(x, y)) {
                continue;
            }
            const double centerValue = grid.value(x, y);
            if (std::isfinite(centerValue) && centerValue > maxBridgeValue) {
                continue;
            }
            if (ownerAt(x, y) != -2) {
                continue;
            }

            int firstOwner = 0;
            bool hasDifferentOwners = false;
            int total = 0;
            int supported = 0;
            for (int yy = y - radius; yy <= y + radius; ++yy) {
                for (int xx = x - radius; xx <= x + radius; ++xx) {
                    if (!grid.inBounds(xx, yy)) {
                        continue;
                    }
                    if (activeMask.get(xx, yy)) {
                        ++total;
                        if (attributeMask.get(xx, yy)) {
                            ++supported;
                        }
                    }
                    const int owner = ownerAt(xx, yy);
                    if (owner <= 0) {
                        continue;
                    }
                    if (firstOwner == 0) {
                        firstOwner = owner;
                    } else if (owner != firstOwner) {
                        hasDifferentOwners = true;
                    }
                }
            }

            if (!hasDifferentOwners) {
                continue;
            }

            const double ratio = total > 0
                ? static_cast<double>(supported) / static_cast<double>(total)
                : 0.0;
            if (ratio < supportRatio) {
                cutMask.set(x, y, true);
            }
        }
    }

    return cutMask;
}

struct RootAntiMergeValidationStats {
    int candidateCutCells = 0;
    int acceptedCutCells = 0;
    int rejectedCutCells = 0;
    int acceptedComponents = 0;
    int rejectedComponents = 0;
    int weakGuardRejectedCutCells = 0;
    int weakGuardRejectedComponents = 0;
};

struct LocalAntiMergeValidationStats {
    int candidateCutCells = 0;
    int acceptedCutCells = 0;
    int rejectedCutCells = 0;
    int acceptedComponents = 0;
    int rejectedComponents = 0;
};

BinaryMask validateRootAntiMergeCutMask(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const BinaryMask& candidateCutMask,
    const TraceOptions& options,
    RootAntiMergeValidationStats& stats) {
    BinaryMask acceptedCutMask(grid.nx(), grid.ny());
    stats.candidateCutCells = candidateCutMask.countActive();
    if (stats.candidateCutCells <= 0) {
        return acceptedCutMask;
    }

    const int width = grid.nx();
    const int height = grid.ny();
    const int minPartCells = options.rootAntiMergeMinPartCells > 0
        ? options.rootAntiMergeMinPartCells
        : std::max(12, std::max(options.adaptiveAreaMinCells * 3, options.beltRadius * options.beltRadius * 2));
    const int weakGuardRadius = std::max(0, options.rootAntiMergeWeakGuardRadius);
    const double weakGuardThreshold = options.rootAntiMergeWeakGuardThreshold;

    auto indexOf = [width](int x, int y) {
        return y * width + x;
    };

    auto isWeakGuardedCandidate = [&grid, weakGuardRadius, weakGuardThreshold](int x, int y) {
        if (weakGuardRadius <= 0 || weakGuardThreshold <= 0.0) {
            return false;
        }
        for (int yy = y - weakGuardRadius; yy <= y + weakGuardRadius; ++yy) {
            for (int xx = x - weakGuardRadius; xx <= x + weakGuardRadius; ++xx) {
                if (!grid.inBounds(xx, yy)) {
                    continue;
                }
                const double value = grid.value(xx, yy);
                if (std::isfinite(value) && value >= weakGuardThreshold) {
                    return true;
                }
            }
        }
        return false;
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    std::vector<unsigned char> visited(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> inComponent(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> partVisited(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> weakGuardedCut(static_cast<std::size_t>(width * height), 0);

    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const std::size_t startIdx = static_cast<std::size_t>(indexOf(x0, y0));
            if (!activeMask.get(x0, y0) || visited[startIdx]) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            queue.push({x0, y0});
            visited[startIdx] = 1;
            int componentCutCells = 0;
            int componentWeakGuardedCutCells = 0;
            int componentEligibleCutCells = 0;

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                inComponent[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 1;
                if (candidateCutMask.get(cell.x, cell.y)) {
                    ++componentCutCells;
                    const bool weakGuarded = isWeakGuardedCandidate(cell.x, cell.y);
                    if (weakGuarded) {
                        ++componentWeakGuardedCutCells;
                        weakGuardedCut[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 1;
                    } else {
                        ++componentEligibleCutCells;
                    }
                }

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                    if (!activeMask.get(nxCell, nyCell) || visited[pos]) {
                        continue;
                    }
                    visited[pos] = 1;
                    queue.push({nxCell, nyCell});
                }
            }

            if (componentCutCells <= 0) {
                for (const Cell& cell : cells) {
                    inComponent[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 0;
                    weakGuardedCut[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 0;
                }
                continue;
            }

            int partCount = 0;
            int majorPartCount = 0;
            const double minMajorPartCells = static_cast<double>(cells.size()) *
                std::max(0.0, options.rootAntiMergeMinPartRatio);

            for (const Cell& startCell : cells) {
                const std::size_t startPos = static_cast<std::size_t>(indexOf(startCell.x, startCell.y));
                if (partVisited[startPos] ||
                    (candidateCutMask.get(startCell.x, startCell.y) && !weakGuardedCut[startPos])) {
                    continue;
                }

                std::queue<Cell> partQueue;
                partQueue.push(startCell);
                partVisited[startPos] = 1;
                int partCells = 0;

                while (!partQueue.empty()) {
                    const Cell cell = partQueue.front();
                    partQueue.pop();
                    ++partCells;

                    for (const auto& offset : offsets) {
                        const int nxCell = cell.x + offset[0];
                        const int nyCell = cell.y + offset[1];
                        if (!grid.inBounds(nxCell, nyCell)) {
                            continue;
                        }
                        const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                        if (!inComponent[pos] || partVisited[pos] ||
                            (candidateCutMask.get(nxCell, nyCell) && !weakGuardedCut[pos])) {
                            continue;
                        }
                        partVisited[pos] = 1;
                        partQueue.push({nxCell, nyCell});
                    }
                }

                ++partCount;
                if (partCells >= minPartCells && static_cast<double>(partCells) >= minMajorPartCells) {
                    ++majorPartCount;
                }
            }

            const bool accepted = componentEligibleCutCells > 0 && partCount >= 2 && majorPartCount >= 2;
            if (accepted) {
                ++stats.acceptedComponents;
                stats.acceptedCutCells += componentEligibleCutCells;
                stats.rejectedCutCells += componentWeakGuardedCutCells;
            } else {
                ++stats.rejectedComponents;
                stats.rejectedCutCells += componentCutCells;
            }
            if (componentWeakGuardedCutCells > 0) {
                stats.weakGuardRejectedCutCells += componentWeakGuardedCutCells;
                ++stats.weakGuardRejectedComponents;
            }

            for (const Cell& cell : cells) {
                const std::size_t pos = static_cast<std::size_t>(indexOf(cell.x, cell.y));
                if (candidateCutMask.get(cell.x, cell.y) && !weakGuardedCut[pos] && accepted) {
                    acceptedCutMask.set(cell.x, cell.y, true);
                }
                inComponent[pos] = 0;
                partVisited[pos] = 0;
                weakGuardedCut[pos] = 0;
            }
        }
    }

    return acceptedCutMask;
}

BinaryMask buildLocalAntiMergeCutMask(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const TraceOptions& options,
    LocalAntiMergeValidationStats& stats) {
    BinaryMask acceptedCutMask(grid.nx(), grid.ny());
    const int width = grid.nx();
    const int height = grid.ny();
    if (width <= 0 || height <= 0 || !options.localAntiMerge) {
        return acceptedCutMask;
    }

    const int minPartCells = std::max(1, options.localAntiMergeMinPartCells);
    const int minBridgeCells = std::max(1, options.localAntiMergeMinBridgeCells);
    const int bridgeRadius = std::max(0, options.localAntiMergeBridgeRadius);
    const double minPartRatio = std::max(0.0, options.localAntiMergeMinPartRatio);
    const double minLowRatio = std::max(0.0, options.localAntiMergeMinLowRatio);
    const double lowThreshold = options.localAntiMergeLowThreshold;

    auto indexOf = [width](int x, int y) {
        return y * width + x;
    };

    auto isLowValue = [&grid, lowThreshold](int x, int y) {
        const double value = grid.value(x, y);
        return !std::isfinite(value) || value <= lowThreshold;
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    std::vector<unsigned char> visited(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> inComponent(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> nonLowVisited(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> splitVisited(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> candidateInComponent(static_cast<std::size_t>(width * height), 0);
    std::vector<int> labels(static_cast<std::size_t>(width * height), 0);

    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const std::size_t startIdx = static_cast<std::size_t>(indexOf(x0, y0));
            if (!activeMask.get(x0, y0) || visited[startIdx]) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            queue.push({x0, y0});
            visited[startIdx] = 1;
            int lowCellCount = 0;

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                inComponent[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 1;
                if (isLowValue(cell.x, cell.y)) {
                    ++lowCellCount;
                }

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                    if (!activeMask.get(nxCell, nyCell) || visited[pos]) {
                        continue;
                    }
                    visited[pos] = 1;
                    queue.push({nxCell, nyCell});
                }
            }

            const double componentCells = static_cast<double>(cells.size());
            const bool enoughCells = static_cast<int>(cells.size()) >= minPartCells * 2 + minBridgeCells;
            const double lowRatio = componentCells > 0.0
                ? static_cast<double>(lowCellCount) / componentCells
                : 0.0;
            if (!enoughCells || lowRatio < minLowRatio) {
                for (const Cell& cell : cells) {
                    inComponent[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 0;
                }
                continue;
            }

            int nextLabel = 0;
            std::vector<int> partCells(1, 0);
            for (const Cell& startCell : cells) {
                const std::size_t startPos = static_cast<std::size_t>(indexOf(startCell.x, startCell.y));
                if (nonLowVisited[startPos] || isLowValue(startCell.x, startCell.y)) {
                    continue;
                }

                ++nextLabel;
                partCells.push_back(0);
                std::queue<Cell> partQueue;
                partQueue.push(startCell);
                nonLowVisited[startPos] = 1;
                labels[startPos] = nextLabel;

                while (!partQueue.empty()) {
                    const Cell cell = partQueue.front();
                    partQueue.pop();
                    ++partCells[static_cast<std::size_t>(nextLabel)];

                    for (const auto& offset : offsets) {
                        const int nxCell = cell.x + offset[0];
                        const int nyCell = cell.y + offset[1];
                        if (!grid.inBounds(nxCell, nyCell)) {
                            continue;
                        }
                        const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                        if (!inComponent[pos] || nonLowVisited[pos] || isLowValue(nxCell, nyCell)) {
                            continue;
                        }
                        nonLowVisited[pos] = 1;
                        labels[pos] = nextLabel;
                        partQueue.push({nxCell, nyCell});
                    }
                }
            }

            const double minMajorPartCells = componentCells * minPartRatio;
            std::vector<unsigned char> majorLabel(static_cast<std::size_t>(nextLabel + 1), 0);
            int majorPartCount = 0;
            for (int label = 1; label <= nextLabel; ++label) {
                const int count = partCells[static_cast<std::size_t>(label)];
                if (count >= minPartCells && static_cast<double>(count) >= minMajorPartCells) {
                    majorLabel[static_cast<std::size_t>(label)] = 1;
                    ++majorPartCount;
                }
            }

            std::vector<Cell> candidateCells;
            if (majorPartCount >= 2) {
                std::vector<unsigned char> touched(static_cast<std::size_t>(nextLabel + 1), 0);
                for (const Cell& cell : cells) {
                    if (!isLowValue(cell.x, cell.y)) {
                        continue;
                    }

                    std::vector<int> touchedLabels;
                    for (int yy = cell.y - bridgeRadius; yy <= cell.y + bridgeRadius; ++yy) {
                        for (int xx = cell.x - bridgeRadius; xx <= cell.x + bridgeRadius; ++xx) {
                            if (!grid.inBounds(xx, yy)) {
                                continue;
                            }
                            const int label = labels[static_cast<std::size_t>(indexOf(xx, yy))];
                            if (label > 0 &&
                                majorLabel[static_cast<std::size_t>(label)] &&
                                !touched[static_cast<std::size_t>(label)]) {
                                touched[static_cast<std::size_t>(label)] = 1;
                                touchedLabels.push_back(label);
                            }
                        }
                    }
                    if (touchedLabels.size() >= 2) {
                        candidateCells.push_back(cell);
                    }
                    for (const int label : touchedLabels) {
                        touched[static_cast<std::size_t>(label)] = 0;
                    }
                }
            }

            const int componentCandidateCells = static_cast<int>(candidateCells.size());
            stats.candidateCutCells += componentCandidateCells;
            bool accepted = false;
            if (componentCandidateCells >= minBridgeCells) {
                for (const Cell& cell : candidateCells) {
                    candidateInComponent[static_cast<std::size_t>(indexOf(cell.x, cell.y))] = 1;
                }

                int splitPartCount = 0;
                int splitMajorPartCount = 0;
                for (const Cell& startCell : cells) {
                    const std::size_t startPos = static_cast<std::size_t>(indexOf(startCell.x, startCell.y));
                    if (splitVisited[startPos] || candidateInComponent[startPos]) {
                        continue;
                    }

                    std::queue<Cell> splitQueue;
                    splitQueue.push(startCell);
                    splitVisited[startPos] = 1;
                    int splitCells = 0;

                    while (!splitQueue.empty()) {
                        const Cell cell = splitQueue.front();
                        splitQueue.pop();
                        ++splitCells;

                        for (const auto& offset : offsets) {
                            const int nxCell = cell.x + offset[0];
                            const int nyCell = cell.y + offset[1];
                            if (!grid.inBounds(nxCell, nyCell)) {
                                continue;
                            }
                            const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                            if (!inComponent[pos] || splitVisited[pos] || candidateInComponent[pos]) {
                                continue;
                            }
                            splitVisited[pos] = 1;
                            splitQueue.push({nxCell, nyCell});
                        }
                    }

                    ++splitPartCount;
                    if (splitCells >= minPartCells && static_cast<double>(splitCells) >= minMajorPartCells) {
                        ++splitMajorPartCount;
                    }
                }

                accepted = splitPartCount >= 2 && splitMajorPartCount >= 2;
            }

            if (componentCandidateCells > 0) {
                if (accepted) {
                    ++stats.acceptedComponents;
                    stats.acceptedCutCells += componentCandidateCells;
                    for (const Cell& cell : candidateCells) {
                        acceptedCutMask.set(cell.x, cell.y, true);
                    }
                } else {
                    ++stats.rejectedComponents;
                    stats.rejectedCutCells += componentCandidateCells;
                }
            }

            for (const Cell& cell : cells) {
                const std::size_t pos = static_cast<std::size_t>(indexOf(cell.x, cell.y));
                inComponent[pos] = 0;
                nonLowVisited[pos] = 0;
                splitVisited[pos] = 0;
                candidateInComponent[pos] = 0;
                labels[pos] = 0;
            }
        }
    }

    return acceptedCutMask;
}

std::vector<BranchGroup> groupBranchesByRootTrace(
    const std::vector<const SkeletonBranch*>& branches,
    double maxTurnAngleDegrees) {
    std::vector<BranchGroup> groups;
    if (branches.empty()) {
        return groups;
    }

    std::unordered_map<int, int> indexById;
    indexById.reserve(branches.size());
    for (std::size_t i = 0; i < branches.size(); ++i) {
        indexById[branches[i]->id] = static_cast<int>(i);
    }

    std::unordered_map<std::string, std::vector<BranchEnd>> endsByPoint;
    endsByPoint.reserve(branches.size() * 2);
    for (std::size_t i = 0; i < branches.size(); ++i) {
        const SkeletonBranch& branch = *branches[i];
        if (branch.points.size() < 2) {
            continue;
        }
        const SkeletonPoint& first = branch.points.front();
        const SkeletonPoint& second = branch.points[1];
        endsByPoint[pointKey(first)].push_back({
            static_cast<int>(i),
            {second.x - first.x, second.y - first.y}
        });

        const SkeletonPoint& last = branch.points.back();
        const SkeletonPoint& beforeLast = branch.points[branch.points.size() - 2];
        endsByPoint[pointKey(last)].push_back({
            static_cast<int>(i),
            {beforeLast.x - last.x, beforeLast.y - last.y}
        });
    }

    DisjointSet dsu(static_cast<int>(branches.size()));
    for (const auto& item : endsByPoint) {
        const std::vector<BranchEnd>& ends = item.second;
        if (ends.size() < 2) {
            continue;
        }

        std::vector<unsigned char> used(ends.size(), 0);
        for (std::size_t i = 0; i < ends.size(); ++i) {
            if (used[i]) {
                continue;
            }
            int bestJ = -1;
            double bestAngle = maxTurnAngleDegrees;
            for (std::size_t j = i + 1; j < ends.size(); ++j) {
                if (used[j] || ends[i].branchIndex == ends[j].branchIndex) {
                    continue;
                }
                const SkeletonPoint opposite{-ends[j].direction.x, -ends[j].direction.y};
                const double angle = angleBetweenDirections(ends[i].direction, opposite);
                if (angle <= bestAngle) {
                    bestAngle = angle;
                    bestJ = static_cast<int>(j);
                }
            }
            if (bestJ >= 0) {
                dsu.unite(ends[i].branchIndex, ends[static_cast<std::size_t>(bestJ)].branchIndex);
                used[i] = 1;
                used[static_cast<std::size_t>(bestJ)] = 1;
            }
        }
    }

    std::map<int, std::vector<const SkeletonBranch*>> grouped;
    for (std::size_t i = 0; i < branches.size(); ++i) {
        grouped[dsu.find(static_cast<int>(i))].push_back(branches[i]);
    }

    int nextGroupId = 1;
    groups.reserve(grouped.size());
    for (auto& item : grouped) {
        BranchGroup group;
        group.id = nextGroupId++;
        group.branches = std::move(item.second);
        groups.push_back(std::move(group));
    }
    return groups;
}

bool localGet(const std::vector<unsigned char>& mask, int width, int height, int x, int y) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return false;
    }
    return mask[static_cast<std::size_t>(y * width + x)] != 0;
}

void localSet(std::vector<unsigned char>& mask, int width, int height, int x, int y, bool value) {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    mask[static_cast<std::size_t>(y * width + x)] = value ? 1 : 0;
}

std::vector<unsigned char> localDilate(const std::vector<unsigned char>& mask, int width, int height, int radius) {
    if (radius <= 0) {
        return mask;
    }
    std::vector<unsigned char> out(static_cast<std::size_t>(width * height), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool active = false;
            for (int yy = y - radius; yy <= y + radius && !active; ++yy) {
                for (int xx = x - radius; xx <= x + radius; ++xx) {
                    if (localGet(mask, width, height, xx, yy)) {
                        active = true;
                        break;
                    }
                }
            }
            localSet(out, width, height, x, y, active);
        }
    }
    return out;
}

std::vector<unsigned char> localErode(const std::vector<unsigned char>& mask, int width, int height, int radius) {
    if (radius <= 0) {
        return mask;
    }
    std::vector<unsigned char> out(static_cast<std::size_t>(width * height), 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool active = true;
            for (int yy = y - radius; yy <= y + radius && active; ++yy) {
                for (int xx = x - radius; xx <= x + radius; ++xx) {
                    if (!localGet(mask, width, height, xx, yy)) {
                        active = false;
                        break;
                    }
                }
            }
            localSet(out, width, height, x, y, active);
        }
    }
    return out;
}

std::vector<unsigned char> localClose(const std::vector<unsigned char>& mask, int width, int height, int radius) {
    return localErode(localDilate(mask, width, height, radius), width, height, radius);
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

std::vector<std::vector<Point>> selectExteriorBoundaryRings(
    std::vector<std::vector<Point>> rings,
    int& suppressedInternalRings) {
    std::vector<std::vector<Point>> exterior;
    if (rings.empty()) {
        return exterior;
    }

    std::vector<double> areas(rings.size(), 0.0);
    for (std::size_t i = 0; i < rings.size(); ++i) {
        areas[i] = std::abs(signedPolygonArea(rings[i]));
    }

    std::vector<unsigned char> keep(rings.size(), 0);
    for (std::size_t i = 0; i < rings.size(); ++i) {
        if (rings[i].size() < 3 || areas[i] <= 1e-9) {
            ++suppressedInternalRings;
            continue;
        }

        const Point probe = rings[i].front();
        bool containedByLargerRing = false;
        for (std::size_t j = 0; j < rings.size(); ++j) {
            if (i == j || areas[j] <= areas[i]) {
                continue;
            }
            if (pointInRing(probe, rings[j])) {
                containedByLargerRing = true;
                break;
            }
        }

        if (containedByLargerRing) {
            ++suppressedInternalRings;
        } else {
            keep[i] = 1;
        }
    }

    exterior.reserve(rings.size());
    for (std::size_t i = 0; i < rings.size(); ++i) {
        if (keep[i]) {
            exterior.push_back(std::move(rings[i]));
        }
    }

    return exterior;
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
    const std::vector<Cell>& cells,
    double sum,
    double maxValue,
    std::vector<Point> ring,
    const ComponentStats& stats,
    const TraceOptions& options,
    double keepThreshold,
    TraceResult& result,
    int& nextId) {
    if (signedPolygonArea(ring) <= 0.0) {
        return false;
    }

    Polygon polygon;
    polygon.id = nextId++;
    polygon.points = std::move(ring);
    if (options.simplifyTolerance > 0.0) {
        polygon.points = simplifyRingDouglasPeucker(polygon.points, options.simplifyTolerance);
    }
    if (options.smoothIterations > 0) {
        polygon.points = smoothRingChaikin(polygon.points, options.smoothIterations);
    }
    polygon.area = polygonArea(polygon.points);
    polygon.perimeter = polygonPerimeter(polygon.points);
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

std::vector<std::vector<Point>> traceBoundaryRings(const Grid2D& grid, const std::vector<Cell>& cells) {
    std::vector<unsigned char> componentMask(static_cast<std::size_t>(grid.nx() * grid.ny()), 0);
    for (const Cell& cell : cells) {
        componentMask[static_cast<std::size_t>(cell.y * grid.nx() + cell.x)] = 1;
    }

    std::vector<Edge> edges;
    edges.reserve(cells.size() * 4);
    for (const Cell& cell : cells) {
        const int x = cell.x;
        const int y = cell.y;

        if (!cellSetContains(componentMask, grid.nx(), grid.ny(), x, y - 1)) {
            edges.push_back({{x, y}, {x + 1, y}});
        }
        if (!cellSetContains(componentMask, grid.nx(), grid.ny(), x + 1, y)) {
            edges.push_back({{x + 1, y}, {x + 1, y + 1}});
        }
        if (!cellSetContains(componentMask, grid.nx(), grid.ny(), x, y + 1)) {
            edges.push_back({{x + 1, y + 1}, {x, y + 1}});
        }
        if (!cellSetContains(componentMask, grid.nx(), grid.ny(), x - 1, y)) {
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

        std::vector<Point> ring;
        ring.reserve(ringVertices.size());
        for (const Vertex& vertex : ringVertices) {
            ring.push_back(toWorldPoint(grid, vertex));
        }
        ring = removeCollinear(std::move(ring));
        if (ring.size() >= 3) {
            rings.push_back(std::move(ring));
        }
    }

    return rings;
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
            std::vector<std::vector<Point>> rings = selectExteriorBoundaryRings(
                traceBoundaryRings(grid, cells),
                result.suppressedInternalRings);
            for (std::vector<Point>& ring : rings) {
                finalizeAndAppendPolygon(cells, sum, maxValue, std::move(ring), stats, options, result.effectiveThreshold, result, nextId);
            }
        }
    }
}

void appendPolygonsFromMaskBounds(
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
            std::vector<std::vector<Point>> rings = selectExteriorBoundaryRings(
                traceBoundaryRings(grid, cells),
                result.suppressedInternalRings);
            for (std::vector<Point>& ring : rings) {
                finalizeAndAppendPolygon(cells, sum, maxValue, std::move(ring), stats, options, result.effectiveThreshold, result, nextId);
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
            std::vector<std::vector<Point>> rings = selectExteriorBoundaryRings(
                traceBoundaryRings(grid, cells),
                result.suppressedInternalRings);
            for (std::vector<Point>& ring : rings) {
                const bool kept = finalizeAndAppendPolygon(cells, sum, maxValue, std::move(ring), stats, options, result.effectiveThreshold, result, nextId);
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

            if (static_cast<int>(cells.size()) < options.residualMinCells) {
                continue;
            }
            if (options.residualTraceMaxCells > 0 &&
                static_cast<int>(cells.size()) > options.residualTraceMaxCells) {
                continue;
            }

            const ComponentStats stats = computeComponentStats(cells);
            const int longestAxis = std::max(stats.width, stats.height);
            const double meanValue = cells.empty() ? 0.0 : sum / static_cast<double>(cells.size());
            const bool lineLike = stats.aspect >= options.adaptiveAreaMinAspect ||
                longestAxis >= options.residualTraceMinLength;
            const bool strongEnough = maxValue >= result.effectiveThreshold * options.adaptiveAreaMinMaxRatio ||
                meanValue >= result.effectiveThreshold * options.adaptiveAreaMinMeanRatio;
            bool detachedTrace = false;
            if (!touchesCovered && options.residualTraceDetached) {
                const bool detachedSizeOk = static_cast<int>(cells.size()) >= options.residualDetachedMinCells;
                const bool detachedLineLike = stats.aspect >= options.residualDetachedMinAspect &&
                    longestAxis >= options.residualDetachedMinLength;
                const bool detachedStrongEnough =
                    maxValue >= result.effectiveThreshold * options.residualDetachedMinMaxRatio &&
                    meanValue >= result.effectiveThreshold * options.residualDetachedMinMeanRatio;
                detachedTrace = detachedSizeOk && detachedLineLike && detachedStrongEnough;
            }
            if ((!touchesCovered && !detachedTrace) || !lineLike || !strongEnough || longestAxis < options.residualTraceMinLength) {
                continue;
            }

            BinaryMask localMask(grid.nx(), grid.ny());
            for (const Cell& cell : cells) {
                localMask.set(cell.x, cell.y, true);
            }
            localMask = bufferMask(localMask, std::max(1, options.beltRadius - 1));
            if (options.attributeConstrainedBelt) {
                localMask = intersectMask(localMask, dilate(residualMask, std::max(1, options.attributeEnvelopeRadius)));
            }
            if (options.postBeltCloseRadius > 0) {
                localMask = closeMask(localMask, options.postBeltCloseRadius);
            }
            for (int y = stats.minY - options.beltRadius - 2; y <= stats.maxY + options.beltRadius + 2; ++y) {
                for (int x = stats.minX - options.beltRadius - 2; x <= stats.maxX + options.beltRadius + 2; ++x) {
                    if (grid.inBounds(x, y) && coveredGuardMask.get(x, y)) {
                        localMask.set(x, y, false);
                    }
                }
            }
            appendPolygonsFromMaskBounds(
                grid,
                localMask,
                stats.minX - options.beltRadius - 2,
                stats.minY - options.beltRadius - 2,
                stats.maxX + options.beltRadius + 2,
                stats.maxY + options.beltRadius + 2,
                options,
                result,
                nextId);
        }
    }
}

std::vector<unsigned char> buildLocalGroupBelt(
    const Grid2D& grid,
    const BranchGroup& group,
    const std::vector<int>& groupOwners,
    const BinaryMask& residualMask,
    const BinaryMask& attributeEnvelopeMask,
    const TraceOptions& options,
    int minX,
    int minY,
    int localWidth,
    int localHeight) {
    std::vector<unsigned char> seed(static_cast<std::size_t>(localWidth * localHeight), 0);
    for (const SkeletonBranch* branch : group.branches) {
        if (!branch) {
            continue;
        }
        for (const SkeletonPoint& point : branch->points) {
            localSet(seed, localWidth, localHeight, point.x - minX, point.y - minY, true);
        }
    }

    std::vector<unsigned char> belt = localDilate(seed, localWidth, localHeight, options.beltRadius);

    auto ownedByGroup = [&](int gx, int gy) {
        if (groupOwners.size() != static_cast<std::size_t>(grid.nx() * grid.ny())) {
            return true;
        }
        const int owner = groupOwners[static_cast<std::size_t>(gy * grid.nx() + gx)];
        return owner == group.id;
    };

    auto applyConstraints = [&](std::vector<unsigned char>& mask) {
        for (int ly = 0; ly < localHeight; ++ly) {
            for (int lx = 0; lx < localWidth; ++lx) {
                const std::size_t localPos = static_cast<std::size_t>(ly * localWidth + lx);
                if (!mask[localPos]) {
                    continue;
                }
                const int gx = minX + lx;
                const int gy = minY + ly;
                if (!grid.inBounds(gx, gy) || !ownedByGroup(gx, gy)) {
                    mask[localPos] = 0;
                    continue;
                }
                if (options.attributeConstrainedBelt && !attributeEnvelopeMask.get(gx, gy)) {
                    mask[localPos] = 0;
                }
            }
        }
    };

    applyConstraints(belt);

    if (options.residualAttributeFill && options.residualFillRadius > 0) {
        std::vector<unsigned char> groupReach = localDilate(seed, localWidth, localHeight, options.residualFillRadius);
        std::vector<unsigned char> residualVisited(static_cast<std::size_t>(localWidth * localHeight), 0);
        constexpr int offsets[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},
            {-1, 0},           {1, 0},
            {-1, 1},  {0, 1},  {1, 1}
        };
        auto localIndex = [localWidth](int x, int y) {
            return y * localWidth + x;
        };

        for (int sy = 0; sy < localHeight; ++sy) {
            for (int sx = 0; sx < localWidth; ++sx) {
                const std::size_t startPos = static_cast<std::size_t>(localIndex(sx, sy));
                if (residualVisited[startPos] || !groupReach[startPos] || belt[startPos]) {
                    continue;
                }
                const int startGx = minX + sx;
                const int startGy = minY + sy;
                if (!grid.inBounds(startGx, startGy) || !ownedByGroup(startGx, startGy) || !residualMask.get(startGx, startGy)) {
                    residualVisited[startPos] = 1;
                    continue;
                }

                std::queue<Cell> queue;
                std::vector<Cell> component;
                queue.push({sx, sy});
                residualVisited[startPos] = 1;
                bool touchesBelt = false;

                while (!queue.empty()) {
                    const Cell cell = queue.front();
                    queue.pop();
                    component.push_back(cell);

                    for (const auto& offset : offsets) {
                        const int nxCell = cell.x + offset[0];
                        const int nyCell = cell.y + offset[1];
                        if (nxCell < 0 || nyCell < 0 || nxCell >= localWidth || nyCell >= localHeight) {
                            continue;
                        }
                        const std::size_t pos = static_cast<std::size_t>(localIndex(nxCell, nyCell));
                        if (belt[pos]) {
                            touchesBelt = true;
                            continue;
                        }
                        if (residualVisited[pos] || !groupReach[pos]) {
                            continue;
                        }
                        const int gx = minX + nxCell;
                        const int gy = minY + nyCell;
                        if (!grid.inBounds(gx, gy) || !ownedByGroup(gx, gy) || !residualMask.get(gx, gy)) {
                            residualVisited[pos] = 1;
                            continue;
                        }
                        residualVisited[pos] = 1;
                        queue.push({nxCell, nyCell});
                    }
                }

                if (touchesBelt && static_cast<int>(component.size()) >= options.residualMinCells) {
                    for (const Cell& cell : component) {
                        localSet(belt, localWidth, localHeight, cell.x, cell.y, true);
                    }
                }
            }
        }
        applyConstraints(belt);
    }

    if (options.postBeltCloseRadius > 0) {
        belt = localClose(belt, localWidth, localHeight, options.postBeltCloseRadius);
        applyConstraints(belt);
    }

    for (const SkeletonBranch* branch : group.branches) {
        if (!branch) {
            continue;
        }
        for (const SkeletonPoint& point : branch->points) {
            if (grid.inBounds(point.x, point.y) && ownedByGroup(point.x, point.y)) {
                localSet(belt, localWidth, localHeight, point.x - minX, point.y - minY, true);
            }
        }
    }

    return belt;
}

BranchTrace makeBranchTrace(const Grid2D& grid, const SkeletonBranch& branch, int minBranchLength) {
    BranchTrace trace;
    trace.id = branch.id;
    trace.pointCount = static_cast<int>(branch.points.size());
    trace.kept = trace.pointCount >= minBranchLength;
    trace.startIsJunction = branch.startIsJunction;
    trace.endIsJunction = branch.endIsJunction;
    trace.points.reserve(branch.points.size());

    double sum = 0.0;
    double maxValue = -std::numeric_limits<double>::infinity();
    for (const SkeletonPoint& point : branch.points) {
        trace.points.push_back(toWorldPoint(grid, point));
        const double value = grid.inBounds(point.x, point.y) ? grid.value(point.x, point.y) : 0.0;
        sum += value;
        maxValue = std::max(maxValue, value);
    }

    trace.meanValue = branch.points.empty() ? 0.0 : sum / static_cast<double>(branch.points.size());
    trace.maxValue = branch.points.empty() ? 0.0 : maxValue;
    for (std::size_t i = 1; i < trace.points.size(); ++i) {
        trace.length += pointDistance(trace.points[i - 1], trace.points[i]);
    }
    return trace;
}

BinaryMask extendThroughWeakAttributes(
    const Grid2D& grid,
    const BinaryMask& strongMask,
    double strongThreshold,
    double weakPercentile,
    int connectRadius,
    int maxWeakComponentCells,
    double minWeakComponentAspect,
    bool positivePercentile) {
    if (weakPercentile <= 0.0 || connectRadius <= 0) {
        return strongMask;
    }

    const double weakThreshold = positivePercentile
        ? computePositivePercentileThreshold(grid, weakPercentile)
        : computePercentileThreshold(grid, weakPercentile);
    if (weakThreshold >= strongThreshold) {
        return strongMask;
    }

    BinaryMask seedMask = dilate(strongMask, connectRadius);
    BinaryMask out = strongMask;
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
            const std::size_t startIdx = static_cast<std::size_t>(indexOf(x0, y0));
            if (visited[startIdx]) {
                continue;
            }
            const double startValue = grid.value(x0, y0);
            if (!std::isfinite(startValue) || startValue < weakThreshold) {
                visited[startIdx] = 1;
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> cells;
            bool touchesSeed = false;
            int minX = x0;
            int maxX = x0;
            int minY = y0;
            int maxY = y0;
            queue.push({x0, y0});
            visited[startIdx] = 1;

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                touchesSeed = touchesSeed || seedMask.get(cell.x, cell.y);
                minX = std::min(minX, cell.x);
                maxX = std::max(maxX, cell.x);
                minY = std::min(minY, cell.y);
                maxY = std::max(maxY, cell.y);

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                    if (visited[pos]) {
                        continue;
                    }
                    const double value = grid.value(nxCell, nyCell);
                    if (!std::isfinite(value) || value < weakThreshold) {
                        visited[pos] = 1;
                        continue;
                    }
                    visited[pos] = 1;
                    queue.push({nxCell, nyCell});
                }
            }

            const int width = std::max(1, maxX - minX + 1);
            const int height = std::max(1, maxY - minY + 1);
            const double aspect = static_cast<double>(std::max(width, height)) /
                static_cast<double>(std::max(1, std::min(width, height)));
            const bool sizeOk = maxWeakComponentCells <= 0 ||
                static_cast<int>(cells.size()) <= maxWeakComponentCells;
            const bool shapeOk = minWeakComponentAspect <= 1.0 || aspect >= minWeakComponentAspect;
            if (touchesSeed && (sizeOk || shapeOk)) {
                for (const Cell& cell : cells) {
                    out.set(cell.x, cell.y, true);
                }
            }
        }
    }

    return out;
}

} // namespace

TraceResult traceFaultPolygons(const Grid2D& grid, const TraceOptions& options) {
    if (grid.empty()) {
        throw std::runtime_error("Cannot trace an empty grid.");
    }
    if (options.minCells < 1) {
        throw std::runtime_error("minCells must be at least 1.");
    }
    if (options.openRadius < 0 || options.closeRadius < 0) {
        throw std::runtime_error("Morphology radii must be non-negative.");
    }
    if (options.weakPercentile < 0.0 || options.weakPercentile >= 1.0) {
        throw std::runtime_error("weakPercentile must be 0 or between 0 and 1.");
    }
    if (options.weakConnectRadius < 0) {
        throw std::runtime_error("weakConnectRadius must be non-negative.");
    }
    if (options.residualPercentile < 0.0 || options.residualPercentile >= 1.0) {
        throw std::runtime_error("residualPercentile must be 0 or between 0 and 1.");
    }
    if (options.beltRadius < 1) {
        throw std::runtime_error("beltRadius must be at least 1.");
    }
    if (options.postBeltCloseRadius < 0) {
        throw std::runtime_error("postBeltCloseRadius must be non-negative.");
    }
    if (options.attributeEnvelopeRadius < 0) {
        throw std::runtime_error("attributeEnvelopeRadius must be non-negative.");
    }
    if (options.residualFillRadius < 0) {
        throw std::runtime_error("residualFillRadius must be non-negative.");
    }
    if (options.residualMinCells < 1) {
        throw std::runtime_error("residualMinCells must be at least 1.");
    }
    if (options.residualTraceMaxCells < 0) {
        throw std::runtime_error("residualTraceMaxCells must be non-negative.");
    }
    if (options.residualCoveredGuardRadius < 0) {
        throw std::runtime_error("residualCoveredGuardRadius must be non-negative.");
    }
    if (options.residualDetachedMinCells < 1 || options.residualDetachedMinLength < 1) {
        throw std::runtime_error("residual detached trace minimums must be positive.");
    }
    if (options.minBranchLength < 1) {
        throw std::runtime_error("minBranchLength must be at least 1.");
    }
    if (options.minJunctionBranchLength < 1) {
        throw std::runtime_error("minJunctionBranchLength must be at least 1.");
    }
    if (options.maxGapDistance < 0) {
        throw std::runtime_error("maxGapDistance must be non-negative.");
    }
    if (options.maxGapAngle < 0.0 || options.maxGapAngle > 180.0) {
        throw std::runtime_error("maxGapAngle must be between 0 and 180 degrees.");
    }
    if (options.rootAntiMergeRadius < 0) {
        throw std::runtime_error("rootAntiMergeRadius must be non-negative.");
    }
    if (options.rootAntiMergeSupportRatio < 0.0 || options.rootAntiMergeSupportRatio > 1.0) {
        throw std::runtime_error("rootAntiMergeSupportRatio must be between 0 and 1.");
    }
    if (options.rootAntiMergeMaxBridgeValue < 0.0) {
        throw std::runtime_error("rootAntiMergeMaxBridgeValue must be non-negative.");
    }
    if (options.rootAntiMergeMinPartCells < 0) {
        throw std::runtime_error("rootAntiMergeMinPartCells must be non-negative.");
    }
    if (options.rootAntiMergeMinPartRatio < 0.0 || options.rootAntiMergeMinPartRatio > 0.5) {
        throw std::runtime_error("rootAntiMergeMinPartRatio must be between 0 and 0.5.");
    }
    if (options.rootAntiMergeWeakGuardThreshold < 0.0) {
        throw std::runtime_error("rootAntiMergeWeakGuardThreshold must be non-negative.");
    }
    if (options.rootAntiMergeWeakGuardRadius < 0) {
        throw std::runtime_error("rootAntiMergeWeakGuardRadius must be non-negative.");
    }
    if (options.localAntiMergeLowThreshold < 0.0) {
        throw std::runtime_error("localAntiMergeLowThreshold must be non-negative.");
    }
    if (options.localAntiMergeBridgeRadius < 0) {
        throw std::runtime_error("localAntiMergeBridgeRadius must be non-negative.");
    }
    if (options.localAntiMergeMinPartCells < 1) {
        throw std::runtime_error("localAntiMergeMinPartCells must be at least 1.");
    }
    if (options.localAntiMergeMinPartRatio < 0.0 || options.localAntiMergeMinPartRatio > 0.5) {
        throw std::runtime_error("localAntiMergeMinPartRatio must be between 0 and 0.5.");
    }
    if (options.localAntiMergeMinBridgeCells < 1) {
        throw std::runtime_error("localAntiMergeMinBridgeCells must be at least 1.");
    }
    if (options.localAntiMergeMinLowRatio < 0.0 || options.localAntiMergeMinLowRatio > 1.0) {
        throw std::runtime_error("localAntiMergeMinLowRatio must be between 0 and 1.");
    }
    if (options.simplifyTolerance < 0.0) {
        throw std::runtime_error("simplifyTolerance must be non-negative.");
    }
    if (options.smoothIterations < 0 || options.smoothIterations > 5) {
        throw std::runtime_error("smoothIterations must be between 0 and 5.");
    }

    const int nx = grid.nx();
    const int ny = grid.ny();

    SegmentationOptions segmentationOptions;
    segmentationOptions.thresholdMode = options.thresholdMode;
    segmentationOptions.threshold = options.threshold;
    segmentationOptions.percentile = options.percentile;
    segmentationOptions.openRadius = options.openRadius;
    segmentationOptions.closeRadius = options.closeRadius;
    SegmentationResult segmentation = segmentFaultAttributes(grid, segmentationOptions);

    TraceResult result;
    result.effectiveThreshold = segmentation.effectiveThreshold;
    const double residualSupportThreshold =
        options.residualAttributeFill && options.residualPercentile > 0.0
            ? (options.thresholdMode == ThresholdMode::PositivePercentile
                ? computePositivePercentileThreshold(grid, options.residualPercentile)
                : computePercentileThreshold(grid, options.residualPercentile))
            : result.effectiveThreshold;
    BinaryMask activeMask = segmentation.mask;
    activeMask = extendThroughWeakAttributes(
        grid,
        activeMask,
        result.effectiveThreshold,
        options.weakPercentile,
        options.weakConnectRadius,
        options.maxWeakComponentCells,
        options.minWeakComponentAspect,
        options.thresholdMode == ThresholdMode::PositivePercentile);
    if (options.closeRadius > 0) {
        activeMask = closeMask(activeMask, options.closeRadius);
    }
    const BinaryMask attributeMask = activeMask;
    BinaryMask residualMask = attributeMask;
    if (options.residualAttributeFill && options.residualPercentile > 0.0) {
        SegmentationOptions residualSegmentationOptions;
        residualSegmentationOptions.thresholdMode = options.thresholdMode == ThresholdMode::PositivePercentile
            ? ThresholdMode::PositivePercentile
            : ThresholdMode::Percentile;
        residualSegmentationOptions.percentile = options.residualPercentile;
        residualSegmentationOptions.openRadius = 0;
        residualSegmentationOptions.closeRadius = 0;
        residualMask = segmentFaultAttributes(grid, residualSegmentationOptions).mask;
    }
    if (options.traceMode == TraceMode::RidgeBelt) {
        BinaryMask skeleton = skeletonizeZhangSuen(activeMask);
        SkeletonGraph graph = buildSkeletonGraph(skeleton);
        result.skeletonEndpoints = static_cast<int>(graph.endpoints.size());
        result.skeletonJunctions = static_cast<int>(graph.junctions.size());
        result.skeletonBranches = static_cast<int>(graph.branches.size());

        GapSupportContext supportContext;
        supportContext.grid = &grid;
        supportContext.supportThreshold = residualSupportThreshold;
        supportContext.minSupportRatio = options.minGapSupportRatio;
        GapConnectionResult connection = connectEndpointGapsWithStats(
            skeleton,
            graph,
            options.maxGapDistance,
            options.maxGapAngle,
            gapHasAttributeSupport,
            &supportContext);
        result.connectedGaps = connection.connectedGaps;
        SkeletonGraph connectedGraph = buildSkeletonGraph(connection.mask);
        result.branches.reserve(connectedGraph.branches.size());
        BinaryMask branchMask(nx, ny);
        std::vector<const SkeletonBranch*> keptBranchRefs;
        keptBranchRefs.reserve(connectedGraph.branches.size());
        for (const SkeletonBranch& branch : connectedGraph.branches) {
            BranchTrace branchTrace = makeBranchTrace(grid, branch, options.minBranchLength);
            const bool touchesJunction = branch.startIsJunction || branch.endIsJunction;
            const bool keepByLength = branchTrace.pointCount >= options.minBranchLength;
            const bool keepByJunction = touchesJunction &&
                branchTrace.pointCount >= options.minJunctionBranchLength &&
                branchTrace.meanValue >= options.minJunctionBranchMean;
            branchTrace.kept = keepByLength || keepByJunction;
            if (branchTrace.kept) {
                ++result.keptBranches;
                keptBranchRefs.push_back(&branch);
                for (const SkeletonPoint& point : branch.points) {
                    branchMask.set(point.x, point.y, true);
                }
            } else {
                ++result.prunedBranches;
            }
            result.branches.push_back(std::move(branchTrace));
        }
        int nextId = 1;
        if (options.separateBranchBelts) {
            const int ownershipRadius = options.beltRadius + options.postBeltCloseRadius + options.attributeEnvelopeRadius + 3;
            const std::vector<BranchGroup> branchGroups = groupBranchesByRootTrace(keptBranchRefs, options.maxGapAngle);
            const std::vector<int> groupOwners = buildGroupOwnershipMap(nx, ny, branchGroups, ownershipRadius);
            const BinaryMask attributeEnvelopeMask = options.attributeConstrainedBelt && options.attributeEnvelopeRadius > 0
                ? dilate(attributeMask, options.attributeEnvelopeRadius)
                : attributeMask;
            BinaryMask candidateCoveredMask(nx, ny);
            BinaryMask coveredMask(nx, ny);
            for (const BranchGroup& group : branchGroups) {
                if (group.branches.empty()) {
                    continue;
                }
                int minX = grid.nx();
                int minY = grid.ny();
                int maxX = -1;
                int maxY = -1;
                for (const SkeletonBranch* branch : group.branches) {
                    if (!branch) {
                        continue;
                    }
                    for (const SkeletonPoint& point : branch->points) {
                        minX = std::min(minX, point.x);
                        minY = std::min(minY, point.y);
                        maxX = std::max(maxX, point.x);
                        maxY = std::max(maxY, point.y);
                    }
                }
                const int margin = options.beltRadius + options.postBeltCloseRadius + options.attributeEnvelopeRadius + 3;
                minX = std::max(0, minX - margin);
                minY = std::max(0, minY - margin);
                maxX = std::min(grid.nx() - 1, maxX + margin);
                maxY = std::min(grid.ny() - 1, maxY + margin);
                if (minX > maxX || minY > maxY) {
                    continue;
                }
                const int localWidth = maxX - minX + 1;
                const int localHeight = maxY - minY + 1;
                std::vector<unsigned char> localBelt = buildLocalGroupBelt(
                    grid,
                    group,
                    groupOwners,
                    residualMask,
                    attributeEnvelopeMask,
                    options,
                    minX,
                    minY,
                    localWidth,
                    localHeight);
                for (int ly = 0; ly < localHeight; ++ly) {
                    for (int lx = 0; lx < localWidth; ++lx) {
                        if (localBelt[static_cast<std::size_t>(ly * localWidth + lx)]) {
                            candidateCoveredMask.set(minX + lx, minY + ly, true);
                        }
                    }
                }
                appendPolygonsFromLocalMask(
                    grid,
                    localBelt,
                    minX,
                    minY,
                    localWidth,
                    localHeight,
                    options,
                    result,
                    nextId,
                    &coveredMask);
            }
            appendResidualTracePolygons(
                grid,
                residualMask,
                groupOwners,
                coveredMask,
                candidateCoveredMask,
                options,
                result,
                nextId);
            result.activeCellCount = branchMask.countActive();
            return result;
        }

        activeMask = bufferMask(branchMask, options.beltRadius);
        if (options.attributeConstrainedBelt) {
            BinaryMask envelopeMask = options.attributeEnvelopeRadius > 0
                ? dilate(attributeMask, options.attributeEnvelopeRadius)
                : attributeMask;
            activeMask = intersectMask(activeMask, envelopeMask);
        }
        if (options.postBeltCloseRadius > 0) {
            activeMask = closeMask(activeMask, options.postBeltCloseRadius);
        }
        if (options.rootAntiMerge && !keptBranchRefs.empty()) {
            const int ownershipRadius = options.beltRadius + options.postBeltCloseRadius + options.attributeEnvelopeRadius + 3;
            const std::vector<BranchGroup> branchGroups = groupBranchesByRootTrace(keptBranchRefs, options.maxGapAngle);
            const std::vector<int> groupOwners = buildGroupOwnershipMap(nx, ny, branchGroups, ownershipRadius);
            const BinaryMask candidateCutMask = buildRootAntiMergeCutMask(
                grid,
                activeMask,
                attributeMask,
                groupOwners,
                options.rootAntiMergeRadius,
                options.rootAntiMergeSupportRatio,
                options.rootAntiMergeMaxBridgeValue);
            RootAntiMergeValidationStats validationStats;
            const BinaryMask cutMask = validateRootAntiMergeCutMask(
                grid,
                activeMask,
                candidateCutMask,
                options,
                validationStats);
            result.rootAntiMergeCandidateCutCells = validationStats.candidateCutCells;
            result.rootAntiMergeCutCells = validationStats.acceptedCutCells;
            result.rootAntiMergeRejectedCutCells = validationStats.rejectedCutCells;
            result.rootAntiMergeAcceptedComponents = validationStats.acceptedComponents;
            result.rootAntiMergeRejectedComponents = validationStats.rejectedComponents;
            result.rootAntiMergeWeakGuardRejectedCutCells = validationStats.weakGuardRejectedCutCells;
            result.rootAntiMergeWeakGuardRejectedComponents = validationStats.weakGuardRejectedComponents;
            for (int y = 0; y < ny; ++y) {
                for (int x = 0; x < nx; ++x) {
                    if (cutMask.get(x, y)) {
                        activeMask.set(x, y, false);
                    }
                }
            }
        }
        if (options.localAntiMerge) {
            LocalAntiMergeValidationStats validationStats;
            const BinaryMask cutMask = buildLocalAntiMergeCutMask(
                grid,
                activeMask,
                options,
                validationStats);
            result.localAntiMergeCandidateCutCells = validationStats.candidateCutCells;
            result.localAntiMergeCutCells = validationStats.acceptedCutCells;
            result.localAntiMergeRejectedCutCells = validationStats.rejectedCutCells;
            result.localAntiMergeAcceptedComponents = validationStats.acceptedComponents;
            result.localAntiMergeRejectedComponents = validationStats.rejectedComponents;
            for (int y = 0; y < ny; ++y) {
                for (int x = 0; x < nx; ++x) {
                    if (cutMask.get(x, y)) {
                        activeMask.set(x, y, false);
                    }
                }
            }
        }
    }

    int nextId = 1;
    result.activeCellCount = activeMask.countActive();
    appendPolygonsFromMask(grid, activeMask, options, result, nextId);

    return result;
}

} // namespace fpt
