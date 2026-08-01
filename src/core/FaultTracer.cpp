#include "FaultTracer.h"

#include "FinalBridgeGate.h"
#include "GridMetric.h"
#include "PhysicalMorphology.h"
#include "PolygonVectorizer.h"
#include "BranchAttribution.h"
#include "LocalPolygonQuality.h"
#include "SkeletonGraph.h"
#include "Topology.h"
#include "TraceOptionAdapters.h"
#include "TraceOptionsValidation.h"
#include "TracePostProcessor.h"
#include "WeakPathSupport.h"
#include "WeakContinuityConnector.h"
#include "WeakExtensionGate.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace fpt {
namespace {

using TraceClock = std::chrono::steady_clock;

double elapsedMilliseconds(TraceClock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        TraceClock::now() - start).count();
}

struct Cell {
    int x = 0;
    int y = 0;
};

struct OwnershipCell {
    int x = 0;
    int y = 0;
    int ownerId = 0;
};

struct BranchGroup {
    int id = 0;
    std::vector<const SkeletonBranch*> branches;
};

constexpr int kWeakPathStableEvidenceMinCells = 64;
constexpr double kWeakPathMeanEvidenceFraction = 0.25;
constexpr double kWeakPathMinAspectRatio = 0.5;
constexpr int kWeakPathMinMaxWidthCells = 32;
constexpr int kWeakPathWidthRadiusMultiplier = 16;
constexpr double kWeakPathMaxTurnAngleDegrees = 170.0;

struct WeakExtensionStats {
    int componentCount = 0;
    int legacyAcceptedComponents = 0;
    int layeredAcceptedComponents = 0;
    int layeredRejectedComponents = 0;
    int layeredOwnerRejectedComponents = 0;
    int layeredSizeRejectedComponents = 0;
    int layeredShapeRejectedComponents = 0;
    int layeredSupportRejectedComponents = 0;
    int layeredWidthRejectedComponents = 0;
    int legacyOnlyCells = 0;
    int layeredOnlyCells = 0;
    int candidateCells = 0;
    int acceptedCells = 0;
    int rejectedCells = 0;
    int seedCells = 0;
    int seedContactCells = 0;
    int rootConflictRejectedCells = 0;
    int candidateComponents = 0;
    int acceptedComponents = 0;
    int rejectedComponents = 0;
    int partiallyRejectedComponents = 0;
    int evidenceEvaluatedFragments = 0;
    int attributeRejectedFragments = 0;
    int shapeRejectedFragments = 0;
    int directionRejectedFragments = 0;
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

Point toWorldPoint(const Grid2D& grid, const SkeletonPoint& point) {
    return {grid.worldX(point.x + 0.5), grid.worldY(point.y + 0.5)};
}

double pointDistance(const Point& a, const Point& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
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
        if (weakGuardRadius < 0 || weakGuardThreshold <= 0.0) {
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
    const LegacyGreedyBranchPairingResult pairing =
        computeLegacyGreedyBranchPairing(
            branches,
            maxTurnAngleDegrees);
    std::vector<BranchGroup> groups;
    int nextGroupId = 1;
    groups.reserve(pairing.rootBranchIndices.size());
    for (const std::vector<int>& root : pairing.rootBranchIndices) {
        BranchGroup group;
        group.id = nextGroupId++;
        group.branches.reserve(root.size());
        for (const int branchIndex : root) {
            group.branches.push_back(
                branches[static_cast<std::size_t>(branchIndex)]);
        }
        groups.push_back(std::move(group));
    }
    return groups;
}

std::vector<BranchGroup> buildBranchGroupsFromIndices(
    const std::vector<const SkeletonBranch*>& branches,
    const std::vector<std::vector<int>>& rootBranchIndices) {
    std::vector<BranchGroup> groups;
    groups.reserve(rootBranchIndices.size());
    int nextGroupId = 1;
    for (const std::vector<int>& root : rootBranchIndices) {
        BranchGroup group;
        group.id = nextGroupId++;
        group.branches.reserve(root.size());
        for (const int branchIndex : root) {
            if (branchIndex < 0 ||
                branchIndex >= static_cast<int>(branches.size())) {
                throw std::runtime_error(
                    "Branch group references an invalid branch index.");
            }
            group.branches.push_back(
                branches[static_cast<std::size_t>(branchIndex)]);
        }
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

BinaryMask fillResidualAttributesNearActiveBelt(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const BinaryMask& residualMask,
    const TraceOptions& options,
    TraceResult& result) {
    if (!options.residualAttributeFill || options.residualFillRadius <= 0 || residualMask.empty()) {
        return activeMask;
    }

    BinaryMask out = activeMask;
    const int fillRadius = std::max(0, options.residualEdgeFillRadius);
    if (fillRadius <= 0) {
        return activeMask;
    }
    const double minFillValue = result.effectiveThreshold * std::max(0.0, options.residualEdgeFillMinValueRatio);
    const int maxPatchSpan = std::max(1, options.residualEdgeFillMaxPatchSpan);
    const BinaryMask reachMask = dilate(activeMask, fillRadius);
    const int width = grid.nx();
    const int height = grid.ny();
    std::vector<int> labels(static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> activeVisited(static_cast<std::size_t>(width * height), 0);

    auto indexOf = [width](int x, int y) {
        return y * width + x;
    };

    constexpr int offsets[8][2] = {
        {-1, -1}, {0, -1}, {1, -1},
        {-1, 0},           {1, 0},
        {-1, 1},  {0, 1},  {1, 1}
    };

    int nextLabel = 0;
    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const std::size_t startPos = static_cast<std::size_t>(indexOf(x0, y0));
            if (!activeMask.get(x0, y0) || activeVisited[startPos]) {
                continue;
            }
            ++nextLabel;
            std::queue<Cell> queue;
            queue.push({x0, y0});
            activeVisited[startPos] = 1;
            labels[startPos] = nextLabel;

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell) || !activeMask.get(nxCell, nyCell)) {
                        continue;
                    }
                    const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                    if (activeVisited[pos]) {
                        continue;
                    }
                    activeVisited[pos] = 1;
                    labels[pos] = nextLabel;
                    queue.push({nxCell, nyCell});
                }
            }
        }
    }

    auto isCandidate = [&](int x, int y) {
        const double value = grid.value(x, y);
        return grid.inBounds(x, y) &&
            !activeMask.get(x, y) &&
            residualMask.get(x, y) &&
            std::isfinite(value) &&
            value >= minFillValue;
    };

    std::vector<unsigned char> visited(static_cast<std::size_t>(width * height), 0);

    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const std::size_t startPos = static_cast<std::size_t>(indexOf(x0, y0));
            if (visited[startPos] || !isCandidate(x0, y0)) {
                continue;
            }

            std::queue<Cell> queue;
            std::vector<Cell> component;
            queue.push({x0, y0});
            visited[startPos] = 1;
            double sum = 0.0;
            double maxValue = -std::numeric_limits<double>::infinity();
            bool touchesReach = false;
            int minX = x0;
            int minY = y0;
            int maxX = x0;
            int maxY = y0;
            std::vector<int> touchedLabels;

            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                component.push_back(cell);
                const double value = grid.value(cell.x, cell.y);
                sum += value;
                maxValue = std::max(maxValue, value);
                touchesReach = touchesReach || reachMask.get(cell.x, cell.y);
                minX = std::min(minX, cell.x);
                minY = std::min(minY, cell.y);
                maxX = std::max(maxX, cell.x);
                maxY = std::max(maxY, cell.y);

                for (const auto& offset : offsets) {
                    const int nxCell = cell.x + offset[0];
                    const int nyCell = cell.y + offset[1];
                    if (!grid.inBounds(nxCell, nyCell)) {
                        continue;
                    }
                    const int label = labels[static_cast<std::size_t>(indexOf(nxCell, nyCell))];
                    if (label > 0 &&
                        std::find(touchedLabels.begin(), touchedLabels.end(), label) == touchedLabels.end()) {
                        touchedLabels.push_back(label);
                    }
                    if (activeMask.get(nxCell, nyCell)) {
                        continue;
                    }
                    const std::size_t pos = static_cast<std::size_t>(indexOf(nxCell, nyCell));
                    if (visited[pos] || !isCandidate(nxCell, nyCell)) {
                        continue;
                    }
                    visited[pos] = 1;
                    queue.push({nxCell, nyCell});
                }
            }

            result.residualFillCandidateCells += static_cast<int>(component.size());
            const int componentWidth = maxX - minX + 1;
            const int componentHeight = maxY - minY + 1;
            const double meanValue = component.empty() ? 0.0 : sum / static_cast<double>(component.size());
            const bool sizeOk = static_cast<int>(component.size()) >= options.residualMinCells &&
                (options.residualTraceMaxCells <= 0 || static_cast<int>(component.size()) <= options.residualTraceMaxCells);
            const bool localPatch = componentWidth <= maxPatchSpan &&
                componentHeight <= maxPatchSpan;
            double meanX = 0.0;
            double meanY = 0.0;
            for (const Cell& cell : component) {
                meanX += static_cast<double>(cell.x);
                meanY += static_cast<double>(cell.y);
            }
            if (!component.empty()) {
                meanX /= static_cast<double>(component.size());
                meanY /= static_cast<double>(component.size());
            }
            double covarianceXX = 0.0;
            double covarianceXY = 0.0;
            double covarianceYY = 0.0;
            for (const Cell& cell : component) {
                const double dx = static_cast<double>(cell.x) - meanX;
                const double dy = static_cast<double>(cell.y) - meanY;
                covarianceXX += dx * dx;
                covarianceXY += dx * dy;
                covarianceYY += dy * dy;
            }
            const double norm = std::max(1.0, static_cast<double>(component.size() - 1));
            covarianceXX /= norm;
            covarianceXY /= norm;
            covarianceYY /= norm;
            const double trace = covarianceXX + covarianceYY;
            const double determinant = covarianceXX * covarianceYY - covarianceXY * covarianceXY;
            const double discriminant = std::sqrt(std::max(0.0, trace * trace - 4.0 * determinant));
            const double majorEigen = std::max(0.0, 0.5 * (trace + discriminant));
            const double minorEigen = std::max(0.0, 0.5 * (trace - discriminant));
            const double pcaAspect = std::sqrt((majorEigen + 1e-9) / (minorEigen + 1e-9));
            const int maxLineTipSpan = std::max(maxPatchSpan, options.residualTraceMinLength + options.beltRadius + fillRadius);
            const bool lineTipPatch =
                pcaAspect >= options.minWeakComponentAspect &&
                componentWidth <= maxLineTipSpan &&
                componentHeight <= maxLineTipSpan;
            const bool strongEnough = maxValue >= minFillValue && meanValue >= minFillValue;
            const bool fillEligible = touchesReach && sizeOk && (localPatch || lineTipPatch) && strongEnough;
            const bool singleOwner = touchedLabels.size() == 1;
            if (fillEligible && singleOwner) {
                result.residualFillAddedCells += static_cast<int>(component.size());
                for (const Cell& cell : component) {
                    out.set(cell.x, cell.y, true);
                }
            } else if (fillEligible && touchedLabels.size() > 1) {
                std::unordered_map<int, int> localIndexByPosition;
                localIndexByPosition.reserve(component.size() * 2);
                for (std::size_t i = 0; i < component.size(); ++i) {
                    localIndexByPosition[indexOf(component[i].x, component[i].y)] = static_cast<int>(i);
                }

                std::vector<int> owner(component.size(), 0);
                std::vector<int> ownerDistance(component.size(), -1);
                std::queue<int> ownerQueue;

                for (std::size_t i = 0; i < component.size(); ++i) {
                    const Cell& cell = component[i];
                    int firstLabel = 0;
                    bool conflictingSeed = false;
                    for (int yy = cell.y - fillRadius; yy <= cell.y + fillRadius; ++yy) {
                        for (int xx = cell.x - fillRadius; xx <= cell.x + fillRadius; ++xx) {
                            if (!grid.inBounds(xx, yy)) {
                                continue;
                            }
                            const int label = labels[static_cast<std::size_t>(indexOf(xx, yy))];
                            if (label <= 0) {
                                continue;
                            }
                            if (firstLabel == 0) {
                                firstLabel = label;
                            } else if (firstLabel != label) {
                                conflictingSeed = true;
                            }
                        }
                    }
                    if (firstLabel <= 0) {
                        continue;
                    }
                    ownerDistance[i] = 0;
                    if (conflictingSeed) {
                        owner[i] = -2;
                    } else {
                        owner[i] = firstLabel;
                        ownerQueue.push(static_cast<int>(i));
                    }
                }

                while (!ownerQueue.empty()) {
                    const int currentIndex = ownerQueue.front();
                    ownerQueue.pop();
                    if (owner[currentIndex] <= 0) {
                        continue;
                    }
                    const Cell& cell = component[static_cast<std::size_t>(currentIndex)];
                    const int nextDistance = ownerDistance[static_cast<std::size_t>(currentIndex)] + 1;
                    for (const auto& offset : offsets) {
                        const int nxCell = cell.x + offset[0];
                        const int nyCell = cell.y + offset[1];
                        if (!grid.inBounds(nxCell, nyCell)) {
                            continue;
                        }
                        const auto it = localIndexByPosition.find(indexOf(nxCell, nyCell));
                        if (it == localIndexByPosition.end()) {
                            continue;
                        }
                        const int neighborIndex = it->second;
                        if (owner[static_cast<std::size_t>(neighborIndex)] == -2) {
                            continue;
                        }
                        if (ownerDistance[static_cast<std::size_t>(neighborIndex)] < 0) {
                            ownerDistance[static_cast<std::size_t>(neighborIndex)] = nextDistance;
                            owner[static_cast<std::size_t>(neighborIndex)] = owner[static_cast<std::size_t>(currentIndex)];
                            ownerQueue.push(neighborIndex);
                        } else if (ownerDistance[static_cast<std::size_t>(neighborIndex)] == nextDistance &&
                            owner[static_cast<std::size_t>(neighborIndex)] != owner[static_cast<std::size_t>(currentIndex)]) {
                            owner[static_cast<std::size_t>(neighborIndex)] = -2;
                        }
                    }
                }

                std::vector<unsigned char> boundaryBlocked(component.size(), 0);
                for (std::size_t i = 0; i < component.size(); ++i) {
                    if (owner[i] <= 0) {
                        continue;
                    }
                    const Cell& cell = component[i];
                    for (const auto& offset : offsets) {
                        const int nxCell = cell.x + offset[0];
                        const int nyCell = cell.y + offset[1];
                        if (!grid.inBounds(nxCell, nyCell)) {
                            continue;
                        }
                        const auto it = localIndexByPosition.find(indexOf(nxCell, nyCell));
                        if (it == localIndexByPosition.end()) {
                            continue;
                        }
                        const int neighborIndex = it->second;
                        const int neighborOwner = owner[static_cast<std::size_t>(neighborIndex)];
                        if (neighborOwner > 0 && neighborOwner != owner[i]) {
                            boundaryBlocked[i] = 1;
                            boundaryBlocked[static_cast<std::size_t>(neighborIndex)] = 1;
                        }
                    }
                }

                int addedCells = 0;
                for (std::size_t i = 0; i < component.size(); ++i) {
                    if (owner[i] > 0 && !boundaryBlocked[i]) {
                        out.set(component[i].x, component[i].y, true);
                        ++addedCells;
                    }
                }
                if (addedCells > 0) {
                    result.residualFillAddedCells += addedCells;
                } else {
                    ++result.residualFillRejectedComponents;
                }
            } else {
                ++result.residualFillRejectedComponents;
            }
        }
    }

    return out;
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
    bool positivePercentile,
    bool weakPathGate,
    double weakPathSupportRatio,
    int weakPathMaxUnsupportedRun,
    bool layeredShadow,
    bool layeredGate,
    WeakExtensionStats* extensionStats,
    std::vector<WeakExtensionGateEvidence>* layeredEvidence) {
    if (extensionStats) {
        *extensionStats = WeakExtensionStats{};
    }
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
    BinaryMask eligibleExtension(grid.nx(), grid.ny());
    const bool evaluateLayered = layeredShadow || layeredGate;
    WeakExtensionOwnerMap layeredOwners;
    WeakExtensionGateOptions layeredOptions;
    if (evaluateLayered) {
        layeredOwners = buildWeakExtensionOwnerMap(strongMask);
        layeredOptions.maxComponentCells = maxWeakComponentCells > 0
            ? maxWeakComponentCells : std::numeric_limits<int>::max() / 4;
        layeredOptions.smallComponentMaxCells = std::min(
            layeredOptions.maxComponentCells,
            std::max(64, 8 * (connectRadius + 1) * (connectRadius + 1)));
        layeredOptions.ownerSearchRadius = std::max(1, connectRadius);
        layeredOptions.largeMinAspect = std::max(1.0, minWeakComponentAspect);
        layeredOptions.maxEstimatedWidthSamples =
            static_cast<double>(std::max(6, 2 * connectRadius + 4));
        validateWeakExtensionGateOptions(layeredOptions);
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
            const bool legacyAccepted = touchesSeed && (sizeOk || shapeOk);
            bool layeredAccepted = legacyAccepted;
            if (extensionStats) {
                ++extensionStats->componentCount;
                extensionStats->legacyAcceptedComponents += legacyAccepted ? 1 : 0;
            }
            if (evaluateLayered) {
                std::vector<SkeletonPoint> componentPoints;
                componentPoints.reserve(cells.size());
                for (const Cell& cell : cells) {
                    componentPoints.push_back({cell.x, cell.y});
                }
                WeakExtensionGateEvidence evidence = evaluateWeakExtensionComponent(
                    grid,
                    componentPoints,
                    layeredOwners,
                    weakThreshold,
                    strongThreshold,
                    layeredOptions);
                evidence.id = extensionStats ? extensionStats->componentCount :
                    static_cast<int>(layeredEvidence ? layeredEvidence->size() + 1 : 1);
                layeredAccepted = evidence.accepted;
                if (extensionStats) {
                    extensionStats->layeredAcceptedComponents += layeredAccepted ? 1 : 0;
                    extensionStats->layeredRejectedComponents += layeredAccepted ? 0 : 1;
                    extensionStats->legacyOnlyCells +=
                        legacyAccepted && !layeredAccepted ? static_cast<int>(cells.size()) : 0;
                    extensionStats->layeredOnlyCells +=
                        layeredAccepted && !legacyAccepted ? static_cast<int>(cells.size()) : 0;
                    extensionStats->layeredOwnerRejectedComponents +=
                        evidence.decision == "ambiguous_or_missing_seed_owner" ? 1 : 0;
                    extensionStats->layeredSizeRejectedComponents +=
                        evidence.decision == "component_too_large" ? 1 : 0;
                    extensionStats->layeredShapeRejectedComponents +=
                        evidence.decision == "insufficient_physical_linearity" ? 1 : 0;
                    extensionStats->layeredSupportRejectedComponents +=
                        evidence.decision == "insufficient_attribute_support" ? 1 : 0;
                    extensionStats->layeredWidthRejectedComponents +=
                        evidence.decision == "unstable_or_excessive_width" ? 1 : 0;
                }
                if (layeredEvidence && layeredEvidence->size() < 4096) {
                    layeredEvidence->push_back(std::move(evidence));
                }
            }
            const bool componentAccepted = layeredGate ? layeredAccepted : legacyAccepted;
            if (componentAccepted) {
                for (const Cell& cell : cells) {
                    if (weakPathGate) {
                        if (!strongMask.get(cell.x, cell.y)) {
                            eligibleExtension.set(cell.x, cell.y, true);
                        }
                    } else {
                        out.set(cell.x, cell.y, true);
                    }
                }
            }
        }
    }

    if (weakPathGate) {
        WeakPathSupportOptions pathOptions;
        const double clampedRatio = std::clamp(weakPathSupportRatio, 0.0, 1.0);
        pathOptions.supportThreshold = weakThreshold +
            clampedRatio * (strongThreshold - weakThreshold);
        pathOptions.maxUnsupportedRun = weakPathMaxUnsupportedRun;
        pathOptions.seedSearchRadius = connectRadius > 0 ? 1 : 0;
        pathOptions.minEvidenceCells = std::max(
            kWeakPathStableEvidenceMinCells,
            2 * connectRadius + 3);
        pathOptions.minMeanAttribute = weakThreshold +
            kWeakPathMeanEvidenceFraction *
                (pathOptions.supportThreshold - weakThreshold);
        pathOptions.minLowerQuantileAttribute = weakThreshold;
        pathOptions.minAspectRatio = kWeakPathMinAspectRatio;
        pathOptions.maxEstimatedWidth =
            static_cast<double>(std::max(
                kWeakPathMinMaxWidthCells,
                kWeakPathWidthRadiusMultiplier * (connectRadius + 1)));
        pathOptions.maxTurnAngleDegrees =
            kWeakPathMaxTurnAngleDegrees;
        pathOptions.directionWindow = std::max(2, connectRadius + 1);
        const WeakPathSupportResult pathResult = selectWeakPathSupport(
            grid,
            eligibleExtension,
            strongMask,
            pathOptions);
        out = unionMask(strongMask, pathResult.acceptedMask);
        if (extensionStats) {
            extensionStats->candidateCells = pathResult.stats.candidateCellCount;
            extensionStats->acceptedCells = pathResult.stats.acceptedCellCount;
            extensionStats->rejectedCells = pathResult.stats.rejectedCellCount;
            extensionStats->seedCells = pathResult.stats.seedCellCount;
            extensionStats->seedContactCells =
                pathResult.stats.seedContactCellCount;
            extensionStats->rootConflictRejectedCells =
                pathResult.stats.rootConflictRejectedCellCount;
            extensionStats->candidateComponents =
                pathResult.stats.candidateComponentCount;
            extensionStats->acceptedComponents =
                pathResult.stats.acceptedComponentCount;
            extensionStats->rejectedComponents =
                pathResult.stats.rejectedComponentCount;
            extensionStats->partiallyRejectedComponents =
                pathResult.stats.partiallyRejectedComponentCount;
            extensionStats->evidenceEvaluatedFragments =
                pathResult.stats.evidenceEvaluatedFragmentCount;
            extensionStats->attributeRejectedFragments =
                pathResult.stats.attributeRejectedFragmentCount;
            extensionStats->shapeRejectedFragments =
                pathResult.stats.shapeRejectedFragmentCount;
            extensionStats->directionRejectedFragments =
                pathResult.stats.directionRejectedFragmentCount;
        }
    }

    return out;
}

void accumulateWeakContinuityStats(
    WeakContinuityStats& target,
    const WeakContinuityStats& source) {
    target.endpointCount += source.endpointCount;
    target.distanceCandidatePairs += source.distanceCandidatePairs;
    target.directionRejectedPairs += source.directionRejectedPairs;
    target.widthRejectedPairs += source.widthRejectedPairs;
    target.sameComponentRejectedPairs += source.sameComponentRejectedPairs;
    target.preselectedPairs += source.preselectedPairs;
    target.searchedPaths += source.searchedPaths;
    target.foundPaths += source.foundPaths;
    target.noPathRejected += source.noPathRejected;
    target.invalidBackgroundRejected += source.invalidBackgroundRejected;
    target.noEvaluatedSupportRejected += source.noEvaluatedSupportRejected;
    target.attributeRejected += source.attributeRejected;
    target.unsupportedRunRejected += source.unsupportedRunRejected;
    target.curvatureRejected += source.curvatureRejected;
    target.detourRejected += source.detourRejected;
    target.competitionRejected += source.competitionRejected;
    target.endpointReuseRejected += source.endpointReuseRejected;
    target.acceptedConnections += source.acceptedConnections;
    target.globalMatchingComponents += source.globalMatchingComponents;
    target.globalMatchingExactComponents += source.globalMatchingExactComponents;
    target.globalMatchingFallbackComponents += source.globalMatchingFallbackComponents;
    target.globalMatchingNoConnectEndpoints += source.globalMatchingNoConnectEndpoints;
    target.spatialConflictPairs += source.spatialConflictPairs;
    target.spatialConflictRejected += source.spatialConflictRejected;
    target.finiteBackgroundSamples += source.finiteBackgroundSamples;
    target.invalidBackgroundSamples += source.invalidBackgroundSamples;
    target.evaluatedSupportPixels += source.evaluatedSupportPixels;
    target.excludedSkeletonSupportPixels += source.excludedSkeletonSupportPixels;
    target.invalidPathSamples += source.invalidPathSamples;
    target.addedPathPixels += source.addedPathPixels;
}

} // namespace

TraceResult traceFaultPolygons(const Grid2D& grid, const TraceOptions& options) {
    const TraceClock::time_point totalStart = TraceClock::now();
    TraceResult result;
    TraceClock::time_point stageStart = TraceClock::now();
    validateTraceRequest(grid, options);
    result.stageTimings.validationMs = elapsedMilliseconds(stageStart);
    const int nx = grid.nx();
    const int ny = grid.ny();

    stageStart = TraceClock::now();
    SegmentationOptions segmentationOptions;
    segmentationOptions.thresholdMode = options.thresholdMode;
    segmentationOptions.threshold = options.threshold;
    segmentationOptions.percentile = options.percentile;
    segmentationOptions.openRadius = options.openRadius;
    segmentationOptions.closeRadius = options.closeRadius;
    SegmentationResult segmentation = segmentFaultAttributes(grid, segmentationOptions);

    result.effectiveThreshold = segmentation.effectiveThreshold;
    result.primaryThresholdInfo = segmentation.thresholdInfo;
    const auto percentileInfo = [&](double percentile) {
        return options.thresholdMode == ThresholdMode::PositivePercentile
            ? computePositivePercentileThresholdInfo(grid, percentile)
            : computePercentileThresholdInfo(grid, percentile);
    };
    result.residualThresholdInfo =
        options.residualAttributeFill && options.residualPercentile > 0.0
        ? percentileInfo(options.residualPercentile)
        : result.primaryThresholdInfo;
    result.weakThresholdInfo = options.weakPercentile > 0.0
        ? percentileInfo(options.weakPercentile)
        : result.primaryThresholdInfo;
    const double residualSupportThreshold =
        result.residualThresholdInfo.threshold;
    const double weakSupportThreshold = result.weakThresholdInfo.threshold;
    result.stageTimings.segmentationMs = elapsedMilliseconds(stageStart);

    stageStart = TraceClock::now();
    BinaryMask activeMask = segmentation.mask;
    BinaryMask acceptedWeakContinuityPathMask(nx, ny);
    BinaryMask acceptedWeakTailMask(nx, ny);
    WeakExtensionStats weakExtensionStats;
    activeMask = extendThroughWeakAttributes(
        grid,
        activeMask,
        result.effectiveThreshold,
        options.weakPercentile,
        options.weakConnectRadius,
        options.maxWeakComponentCells,
        options.minWeakComponentAspect,
        options.thresholdMode == ThresholdMode::PositivePercentile,
        options.weakPathGate,
        options.weakPathSupportRatio,
        options.weakPathMaxUnsupportedRun,
        options.weakExtensionLayeredShadow,
        options.weakExtensionLayeredGate,
        &weakExtensionStats,
        &result.weakExtensionLayeredEvidence);
    result.weakPathCandidateCells = weakExtensionStats.candidateCells;
    result.weakPathAcceptedCells = weakExtensionStats.acceptedCells;
    result.weakPathRejectedCells = weakExtensionStats.rejectedCells;
    result.weakPathSeedCells = weakExtensionStats.seedCells;
    result.weakPathSeedContactCells = weakExtensionStats.seedContactCells;
    result.weakPathRootConflictRejectedCells =
        weakExtensionStats.rootConflictRejectedCells;
    result.weakPathCandidateComponents =
        weakExtensionStats.candidateComponents;
    result.weakPathAcceptedComponents =
        weakExtensionStats.acceptedComponents;
    result.weakPathRejectedComponents =
        weakExtensionStats.rejectedComponents;
    result.weakPathPartiallyRejectedComponents =
        weakExtensionStats.partiallyRejectedComponents;
    result.weakPathEvidenceEvaluatedFragments =
        weakExtensionStats.evidenceEvaluatedFragments;
    result.weakPathAttributeRejectedFragments =
        weakExtensionStats.attributeRejectedFragments;
    result.weakPathShapeRejectedFragments =
        weakExtensionStats.shapeRejectedFragments;
    result.weakPathDirectionRejectedFragments =
        weakExtensionStats.directionRejectedFragments;
    result.weakExtensionLayeredShadowEvaluated =
        options.weakExtensionLayeredShadow || options.weakExtensionLayeredGate;
    result.weakExtensionComponentCount = weakExtensionStats.componentCount;
    result.weakExtensionLegacyAcceptedComponents =
        weakExtensionStats.legacyAcceptedComponents;
    result.weakExtensionLayeredAcceptedComponents =
        weakExtensionStats.layeredAcceptedComponents;
    result.weakExtensionLayeredRejectedComponents =
        weakExtensionStats.layeredRejectedComponents;
    result.weakExtensionLayeredOwnerRejectedComponents =
        weakExtensionStats.layeredOwnerRejectedComponents;
    result.weakExtensionLayeredSizeRejectedComponents =
        weakExtensionStats.layeredSizeRejectedComponents;
    result.weakExtensionLayeredShapeRejectedComponents =
        weakExtensionStats.layeredShapeRejectedComponents;
    result.weakExtensionLayeredSupportRejectedComponents =
        weakExtensionStats.layeredSupportRejectedComponents;
    result.weakExtensionLayeredWidthRejectedComponents =
        weakExtensionStats.layeredWidthRejectedComponents;
    result.weakExtensionLegacyOnlyCells = weakExtensionStats.legacyOnlyCells;
    result.weakExtensionLayeredOnlyCells = weakExtensionStats.layeredOnlyCells;
    if (options.closeRadius > 0) {
        BinaryMask legacyClosed = closeMask(activeMask, options.closeRadius);
        if (options.physicalMorphologyShadow) {
            const GridMetric metric(grid);
            const double radiusWorld = options.physicalCloseRadiusWorld > 0.0
                ? options.physicalCloseRadiusWorld
                : static_cast<double>(options.closeRadius) * metric.maximumSpacing();
            const BinaryMask physicalClosed =
                closeMaskPhysical(activeMask, metric, radiusWorld);
            int intersectionCells = 0;
            int unionCells = 0;
            for (std::size_t i = 0; i < legacyClosed.values().size(); ++i) {
                const bool legacy = legacyClosed.values()[i] != 0;
                const bool candidate = physicalClosed.values()[i] != 0;
                intersectionCells += legacy && candidate ? 1 : 0;
                unionCells += legacy || candidate ? 1 : 0;
                result.physicalMorphologyAddedCells += candidate && !legacy ? 1 : 0;
                result.physicalMorphologyRemovedCells += legacy && !candidate ? 1 : 0;
            }
            result.physicalMorphologyShadowEvaluated = true;
            result.physicalMorphologyRadiusWorld = radiusWorld;
            result.physicalMorphologyLegacyCells = legacyClosed.countActive();
            result.physicalMorphologyCandidateCells = physicalClosed.countActive();
            result.physicalMorphologyJaccard = unionCells > 0
                ? static_cast<double>(intersectionCells) / static_cast<double>(unionCells)
                : 1.0;
        }
        activeMask = std::move(legacyClosed);
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
    BinaryMask residualTraceMask = residualMask;
    if (options.residualTrace) {
        const double traceThreshold = std::max(
            residualSupportThreshold, weakSupportThreshold);
        result.residualTraceEffectiveThreshold = traceThreshold;
        residualTraceMask = BinaryMask(nx, ny);
        for (int y = 0; y < ny; ++y) {
            for (int x = 0; x < nx; ++x) {
                const double value = grid.value(x, y);
                if (std::isfinite(value) && value >= traceThreshold) {
                    residualTraceMask.set(x, y, true);
                }
            }
        }
    }
    result.stageTimings.preprocessingMs = elapsedMilliseconds(stageStart);

    const TraceClock::time_point ridgeStageStart = TraceClock::now();
    if (options.traceMode == TraceMode::RidgeBelt) {
        BinaryMask skeleton = skeletonizeZhangSuen(activeMask);
        SkeletonGraph graph = buildSkeletonGraph(skeleton);
        result.skeletonEndpoints = static_cast<int>(graph.endpoints.size());
        result.skeletonJunctions = static_cast<int>(graph.junctions.size());
        result.skeletonBranches = static_cast<int>(graph.branches.size());
        if (options.skeletonGraphV2Shadow) {
            const SkeletonGraphV2 graphV2 = buildSkeletonGraphV2(
                skeleton, GridMetric(grid));
            result.skeletonGraphV2ShadowEvaluated = true;
            result.skeletonGraphV2Stats = graphV2.stats;
        }

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
        BinaryMask connectedSkeleton = connection.mask;
        if (options.weakContinuity || options.weakContinuityShadow) {
            const WeakContinuityOptions continuityOptions =
                makeWeakContinuityOptions(options);
            WeakContinuityResult continuity = connectWeakContinuityPaths(
                grid,
                connectedSkeleton,
                attributeMask,
                buildSkeletonGraph(connectedSkeleton),
                std::max(
                    residualSupportThreshold,
                    result.effectiveThreshold *
                        options.weakContinuityReferenceThresholdRatio),
                continuityOptions);
            result.weakContinuityStats = continuity.stats;
            if (options.weakContinuity) {
                for (const WeakContinuityCandidate& candidate : continuity.candidates) {
                    if (!candidate.accepted) {
                        continue;
                    }
                    for (const SkeletonPoint& point : candidate.path) {
                        acceptedWeakContinuityPathMask.set(point.x, point.y, true);
                    }
                }
                connectedSkeleton = std::move(continuity.mask);
            }
            result.weakContinuityCandidates = std::move(continuity.candidates);
        }
        SkeletonGraph connectedGraph = buildSkeletonGraph(connectedSkeleton);
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
        if (options.weakTailCompletion || options.weakTailCompletionShadow ||
            options.weakTailEndpointPortShadow) {
            const WeakTailOptions tailOptions = makeWeakTailOptions(
                options, options.weakTailEndpointPortOwnership);
            WeakTailResult tails = restoreSupportedWeakTails(
                grid,
                connectedSkeleton,
                branchMask,
                result.effectiveThreshold,
                tailOptions);
            result.weakTailStats = tails.stats;
            result.weakTailCandidates = std::move(tails.candidates);
            if (options.weakTailEndpointPortShadow) {
                if (options.weakTailEndpointPortOwnership) {
                    result.weakTailEndpointPortShadowStats = result.weakTailStats;
                    result.weakTailEndpointPortCandidates = result.weakTailCandidates;
                } else {
                    const WeakTailOptions strictOptions =
                        makeWeakTailOptions(options, true);
                    WeakTailResult strictTails = restoreSupportedWeakTails(
                        grid,
                        connectedSkeleton,
                        branchMask,
                        result.effectiveThreshold,
                        strictOptions);
                    result.weakTailEndpointPortShadowStats = strictTails.stats;
                    result.weakTailEndpointPortCandidates =
                        std::move(strictTails.candidates);
                }
                result.weakTailEndpointPortShadowEvaluated = true;
            }
            if (options.weakTailCompletion) {
                branchMask = std::move(tails.mask);
                acceptedWeakTailMask = std::move(tails.acceptedMask);
            }
        }
        if (options.weakContinuity && options.weakContinuitySecondPass) {
            WeakContinuityOptions secondPassOptions =
                makeWeakContinuityOptions(options);
            secondPassOptions.maxDistance = std::min(secondPassOptions.maxDistance, 12);
            secondPassOptions.maxCandidatesPerEndpoint =
                std::min(secondPassOptions.maxCandidatesPerEndpoint, 2);
            secondPassOptions.maxEndpointAngleDegrees =
                std::min(secondPassOptions.maxEndpointAngleDegrees, 30.0);
            secondPassOptions.minSupportRatio =
                std::max(secondPassOptions.minSupportRatio, 0.85);
            secondPassOptions.minMeanSupport =
                std::max(secondPassOptions.minMeanSupport, 0.55);
            secondPassOptions.maxUnsupportedRun =
                std::min(secondPassOptions.maxUnsupportedRun, 2);
            secondPassOptions.maxPathLengthRatio =
                std::min(secondPassOptions.maxPathLengthRatio, 1.25);
            secondPassOptions.maxTurnAngleDegrees =
                std::min(secondPassOptions.maxTurnAngleDegrees, 35.0);
            secondPassOptions.minCompetitionMargin =
                std::max(secondPassOptions.minCompetitionMargin, 0.25);
            BinaryMask provisionalConnectivity =
                bufferMask(branchMask, options.beltRadius);
            if (options.postBeltCloseRadius > 0) {
                provisionalConnectivity = closeMask(
                    provisionalConnectivity,
                    options.postBeltCloseRadius);
            }
            WeakContinuityResult secondPass = connectWeakContinuityPaths(
                grid,
                branchMask,
                attributeMask,
                buildSkeletonGraph(branchMask),
                std::max(
                    residualSupportThreshold,
                    result.effectiveThreshold *
                        options.weakContinuityReferenceThresholdRatio),
                secondPassOptions,
                &provisionalConnectivity);
            int nextCandidateId = 1;
            for (const WeakContinuityCandidate& candidate : result.weakContinuityCandidates) {
                nextCandidateId = std::max(nextCandidateId, candidate.id + 1);
            }
            for (WeakContinuityCandidate& candidate : secondPass.candidates) {
                candidate.id = nextCandidateId++;
                candidate.stage = 2;
                if (candidate.accepted) {
                    for (const SkeletonPoint& point : candidate.path) {
                        acceptedWeakContinuityPathMask.set(point.x, point.y, true);
                    }
                }
                result.weakContinuityCandidates.push_back(std::move(candidate));
            }
            accumulateWeakContinuityStats(
                result.weakContinuityStats,
                secondPass.stats);
            branchMask = std::move(secondPass.mask);
        }
        if (options.branchAttributionShadow ||
            options.branchAttributionCandidateGroups) {
            BranchAttributionOptions attributionOptions;
            attributionOptions.supportThreshold =
                result.effectiveThreshold;
            attributionOptions.maxTurnAngleDegrees =
                options.maxGapAngle;
            BranchAttributionSummary attribution =
                analyzeBranchAttribution(
                    grid,
                    keptBranchRefs,
                    attributionOptions);
            result.branchAttributionJunctionClusters =
                attribution.junctionClusterCount;
            result.branchAttributionAcceptedPairs =
                attribution.acceptedPairCount;
            result.branchAttributionAmbiguousClusters =
                attribution.ambiguousClusterCount;
            result.branchAttributionUnmatchedEnds =
                attribution.unmatchedEndCount;
            result.branchAttributionGreedyFallbackClusters =
                attribution.greedyFallbackClusterCount;
            result.branchAttributionCandidatePairingDifferingClusters =
                attribution.candidatePairingDifferingClusterCount;
            result.branchAttributionCandidateDifferingClusters =
                attribution.candidateDifferingClusterCount;
            result.branchAttributionAcceptedPairingDifferingClusters =
                attribution.acceptedPairingDifferingClusterCount;
            result.branchAttributionAcceptedDifferingClusters =
                attribution.acceptedDifferingClusterCount;
            result.branchAttributionPairingDifferingClusters =
                attribution.pairingDifferingClusterCount;
            result.branchAttributionDifferingClusters =
                attribution.differingClusterCount;
            result.branchAttributionJunctions =
                std::move(attribution.junctions);
        }
        if (options.skeletonGraphV2AttributionShadow) {
            const SkeletonGraphV2 graphV2 = buildSkeletonGraphV2(
                connectedSkeleton, GridMetric(grid));
            BranchAttributionOptions attributionOptions;
            attributionOptions.supportThreshold = result.effectiveThreshold;
            attributionOptions.maxTurnAngleDegrees = options.maxGapAngle;
            result.skeletonGraphV2Attribution =
                compareSkeletonGraphV2Attribution(
                    grid, keptBranchRefs, graphV2, attributionOptions);
            result.skeletonGraphV2AttributionShadowEvaluated = true;
        }
        result.stageTimings.ridgeTracingMs =
            elapsedMilliseconds(ridgeStageStart);
        const TraceClock::time_point beltStageStart = TraceClock::now();
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
            if (options.residualTrace) {
                const BinaryMask finalPolygonCoverage =
                    rasterizeKeptPolygonCoverage(
                        grid, result.polygons, &residualTraceMask);
                appendResidualTracePolygons(
                    grid,
                    residualTraceMask,
                    groupOwners,
                    finalPolygonCoverage,
                    finalPolygonCoverage,
                    options,
                    result,
                    nextId);
            }
            result.activeCellCount = branchMask.countActive();
            result.stageTimings.beltAndAntiMergeMs =
                elapsedMilliseconds(beltStageStart);
            result.stageTimings.beltConstructionMs =
                result.stageTimings.beltAndAntiMergeMs;
            const TraceClock::time_point postStageStart = TraceClock::now();
            applyTracePostProcessing(grid, attributeMask, options, result);
            result.stageTimings.postProcessingMs =
                elapsedMilliseconds(postStageStart);
            result.stageTimings.totalMs = elapsedMilliseconds(totalStart);
            return result;
        }

        const TraceClock::time_point beltConstructionStart = TraceClock::now();
        activeMask = bufferMask(branchMask, options.beltRadius);
        if (options.attributeConstrainedBelt) {
            BinaryMask envelopeSeedMask = attributeMask;
            if (options.preserveObservedSkeletonEnvelope) {
                for (int y = 0; y < ny; ++y) {
                    for (int x = 0; x < nx; ++x) {
                        if (branchMask.get(x, y) && skeleton.get(x, y)) {
                            envelopeSeedMask.set(x, y, true);
                        }
                    }
                }
            }
            if (options.weakContinuity || options.weakTailCompletion) {
                for (int y = 0; y < ny; ++y) {
                    for (int x = 0; x < nx; ++x) {
                        if (acceptedWeakContinuityPathMask.get(x, y) ||
                            acceptedWeakTailMask.get(x, y)) {
                            envelopeSeedMask.set(x, y, true);
                        }
                    }
                }
            }
            BinaryMask envelopeMask = options.attributeEnvelopeRadius > 0
                ? dilate(envelopeSeedMask, options.attributeEnvelopeRadius)
                : envelopeSeedMask;
            activeMask = intersectMask(activeMask, envelopeMask);
        }
        if (options.postBeltCloseRadius > 0) {
            activeMask = closeMask(activeMask, options.postBeltCloseRadius);
        }
        activeMask = fillResidualAttributesNearActiveBelt(
            grid,
            activeMask,
            residualMask,
            options,
            result);
        result.stageTimings.beltConstructionMs =
            elapsedMilliseconds(beltConstructionStart);
        const TraceClock::time_point rootAntiMergeStart = TraceClock::now();
        if (options.rootAntiMerge && !keptBranchRefs.empty()) {
            const int ownershipRadius = options.beltRadius + options.postBeltCloseRadius + options.attributeEnvelopeRadius + 3;
            std::vector<BranchGroup> branchGroups;
            if (options.branchAttributionCandidateGroups) {
                CandidateRootGroupingOptions groupingOptions;
                groupingOptions.maxTurnAngleDegrees =
                    options.maxGapAngle;
                const CandidateRootGroupingResult candidateGrouping =
                    computeCandidateRootGrouping(
                        keptBranchRefs,
                        result.branchAttributionJunctions,
                        groupingOptions);
                result.branchAttributionCandidateGroupingAppliedClusters =
                    candidateGrouping.appliedDifferingClusterCount;
                result.branchAttributionCandidateGroupingAmbiguousFallbackClusters =
                    candidateGrouping.ambiguousFallbackClusterCount;
                result.branchAttributionCandidateGroupingLegacyFallbackClusters =
                    candidateGrouping.legacyFallbackClusterCount;
                result.branchAttributionCandidateGroupingComplexityFallbackClusters =
                    candidateGrouping.complexityFallbackClusterCount;
                result.branchAttributionCandidateGroupingSpanFallbackClusters =
                    candidateGrouping.spanFallbackClusterCount;
                result.branchAttributionCandidateGroupingExcludedInternalBranches =
                    static_cast<int>(
                        candidateGrouping.excludedInternalBranchIndices.size());
                result.branchAttributionCandidateGroupingRootGroups =
                    static_cast<int>(
                        candidateGrouping.rootBranchIndices.size());
                branchGroups = buildBranchGroupsFromIndices(
                    keptBranchRefs,
                    candidateGrouping.rootBranchIndices);
            } else {
                branchGroups = groupBranchesByRootTrace(
                    keptBranchRefs,
                    options.maxGapAngle);
            }
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
            BinaryMask protectedWeakSeedMask(nx, ny);
            if (options.weakContinuity || options.weakTailCompletion) {
                for (int y = 0; y < ny; ++y) {
                    for (int x = 0; x < nx; ++x) {
                        if (acceptedWeakContinuityPathMask.get(x, y) ||
                            acceptedWeakTailMask.get(x, y)) {
                            protectedWeakSeedMask.set(x, y, true);
                        }
                    }
                }
            }
            const BinaryMask protectedWeakPathMask =
                (options.weakContinuity || options.weakTailCompletion)
                ? dilate(protectedWeakSeedMask, 1)
                : BinaryMask(nx, ny);
            for (int y = 0; y < ny; ++y) {
                for (int x = 0; x < nx; ++x) {
                    if (cutMask.get(x, y)) {
                        if (protectedWeakPathMask.get(x, y)) {
                            if (acceptedWeakTailMask.get(x, y)) {
                                ++result.weakTailProtectedFromAntiMergeCells;
                            } else {
                                ++result.weakContinuityProtectedFromAntiMergeCells;
                            }
                            continue;
                        }
                        activeMask.set(x, y, false);
                    }
                }
            }
            result.rootAntiMergeCutCells = std::max(
                0,
                result.rootAntiMergeCutCells -
                    result.weakContinuityProtectedFromAntiMergeCells -
                    result.weakTailProtectedFromAntiMergeCells);
            result.rootAntiMergeRejectedCutCells +=
                result.weakContinuityProtectedFromAntiMergeCells +
                result.weakTailProtectedFromAntiMergeCells;
        }
        result.stageTimings.rootAntiMergeMs =
            elapsedMilliseconds(rootAntiMergeStart);
        const TraceClock::time_point localAntiMergeStart = TraceClock::now();
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
        result.stageTimings.localAntiMergeMs =
            elapsedMilliseconds(localAntiMergeStart);
        const TraceClock::time_point finalWeakContinuityStart =
            TraceClock::now();
        if (options.weakContinuityFinalObjectPass) {
            WeakContinuityOptions finalPassOptions =
                makeWeakContinuityOptions(options);
            finalPassOptions.maxDistance =
                std::min(finalPassOptions.maxDistance, 12);
            finalPassOptions.tangentWindow =
                std::min(finalPassOptions.tangentWindow, 6);
            finalPassOptions.corridorRadius =
                std::min(finalPassOptions.corridorRadius, 5);
            finalPassOptions.maxCandidatesPerEndpoint =
                std::min(finalPassOptions.maxCandidatesPerEndpoint, 2);
            finalPassOptions.maxEndpointAngleDegrees =
                std::min(finalPassOptions.maxEndpointAngleDegrees, 35.0);
            finalPassOptions.maxWidthRatio =
                std::min(finalPassOptions.maxWidthRatio, 2.0);
            finalPassOptions.minSupportRatio =
                std::max(finalPassOptions.minSupportRatio, 0.65);
            finalPassOptions.minMeanSupport =
                std::max(finalPassOptions.minMeanSupport, 0.52);
            finalPassOptions.maxUnsupportedRun =
                std::min(finalPassOptions.maxUnsupportedRun, 2);
            finalPassOptions.maxPathLengthRatio =
                std::min(finalPassOptions.maxPathLengthRatio, 1.25);
            finalPassOptions.maxTurnAngleDegrees =
                std::min(finalPassOptions.maxTurnAngleDegrees, 35.0);
            finalPassOptions.thirdPartySkeletonClearanceRadius =
                std::min(finalPassOptions.thirdPartySkeletonClearanceRadius, 1);
            finalPassOptions.minCompetitionMargin =
                std::max(finalPassOptions.minCompetitionMargin, 0.18);

            TraceClock::time_point finalSubstageStart = TraceClock::now();
            const BinaryMask finalSkeleton = skeletonizeZhangSuen(activeMask);
            const SkeletonGraph finalGraph = buildSkeletonGraph(finalSkeleton);
            result.stageTimings.finalWeakGraphMs =
                elapsedMilliseconds(finalSubstageStart);
            const double finalSupportThreshold = std::max(
                residualSupportThreshold,
                result.effectiveThreshold *
                    options.weakContinuityReferenceThresholdRatio);
            finalSubstageStart = TraceClock::now();
            WeakContinuityResult finalPass = connectWeakContinuityPaths(
                grid,
                finalSkeleton,
                activeMask,
                finalGraph,
                finalSupportThreshold,
                finalPassOptions,
                &activeMask);
            result.stageTimings.finalWeakSearchMs =
                elapsedMilliseconds(finalSubstageStart);
            int nextCandidateId = 1;
            for (const WeakContinuityCandidate& candidate :
                 result.weakContinuityCandidates) {
                nextCandidateId = std::max(nextCandidateId, candidate.id + 1);
            }
            for (WeakContinuityCandidate& candidate : finalPass.candidates) {
                candidate.id = nextCandidateId++;
                candidate.stage = 3;
            }
            finalSubstageStart = TraceClock::now();
            FinalBridgeGateOptions finalGateOptions;
            finalGateOptions.supportThreshold = finalSupportThreshold;
            finalGateOptions.junctionInfluenceDistance =
                std::max(2, finalPassOptions.tangentWindow * 3);
            finalGateOptions.maximumBeltRadius = options.beltRadius;
            finalGateOptions.supportEnvelopeRadius =
                options.attributeEnvelopeRadius;
            FinalBridgeGateResult finalGate = fpt::applyFinalObjectBridgeGates(
                grid,
                activeMask,
                finalGraph,
                finalPass,
                finalGateOptions);
            activeMask = std::move(finalGate.mask);
            result.weakContinuityFinalObjectAcceptedConnections =
                finalGate.acceptedConnections;
            result.weakContinuityFinalObjectRejectedConnections =
                finalGate.rejectedConnections;
            result.weakContinuityFinalObjectAddedEnvelopeCells =
                finalGate.addedEnvelopeCells;
            result.stageTimings.finalWeakGateContextMs =
                finalGate.timings.contextBuildMs;
            result.stageTimings.finalWeakGateOwnershipMs =
                finalGate.timings.ownershipInitializationMs;
            result.stageTimings.finalWeakGateComponentHoleMs =
                finalGate.timings.componentHoleCheckMs;
            result.stageTimings.finalWeakGateEnvelopeMs =
                finalGate.timings.envelopeBuildMs;
            result.stageTimings.finalWeakGateTopologyMs =
                finalGate.timings.localTopologyCheckMs;
            result.stageTimings.finalWeakGateMs =
                elapsedMilliseconds(finalSubstageStart);
            accumulateWeakContinuityStats(
                result.weakContinuityStats,
                finalPass.stats);
            for (WeakContinuityCandidate& candidate : finalPass.candidates) {
                result.weakContinuityCandidates.push_back(std::move(candidate));
            }
        }
        result.stageTimings.finalWeakContinuityMs =
            elapsedMilliseconds(finalWeakContinuityStart);
        result.stageTimings.beltAndAntiMergeMs =
            elapsedMilliseconds(beltStageStart);
    } else {
        result.stageTimings.ridgeTracingMs =
            elapsedMilliseconds(ridgeStageStart);
    }

    const TraceClock::time_point vectorizationStageStart = TraceClock::now();
    int nextId = 1;
    result.activeCellCount = activeMask.countActive();
    appendPolygonsFromMask(grid, activeMask, options, result, nextId);
    if (options.residualTrace) {
        const BinaryMask finalPolygonCoverage =
            rasterizeKeptPolygonCoverage(
                grid, result.polygons, &residualTraceMask);
        appendResidualTracePolygons(
            grid,
            residualTraceMask,
            {},
            finalPolygonCoverage,
            finalPolygonCoverage,
            options,
            result,
            nextId);
    }
    result.stageTimings.vectorizationMs =
        elapsedMilliseconds(vectorizationStageStart);
    const TraceClock::time_point postStageStart = TraceClock::now();
    applyTracePostProcessing(grid, attributeMask, options, result);
    result.stageTimings.postProcessingMs =
        elapsedMilliseconds(postStageStart);
    result.stageTimings.totalMs = elapsedMilliseconds(totalStart);

    return result;
}

} // namespace fpt
