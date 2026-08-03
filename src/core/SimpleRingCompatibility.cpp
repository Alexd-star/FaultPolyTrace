#include "SimpleRingCompatibility.h"

#include "MaskTopology.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fpt {
namespace {

constexpr int kNeighbors4[4][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}
};
constexpr std::size_t kMaximumHoleComponents = 4096;

struct HoleComponent {
    std::vector<int> cells;
    int firstIndex = 0;
};

struct QueueEntry {
    double cost = 0.0;
    int index = 0;
};

struct QueueEntryGreater {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
        if (a.cost != b.cost) {
            return a.cost > b.cost;
        }
        return a.index > b.index;
    }
};

void requireMatchingMask(
    const BinaryMask& reference,
    const BinaryMask& candidate,
    const char* name) {
    if (reference.width() != candidate.width() ||
        reference.height() != candidate.height()) {
        throw std::runtime_error(
            std::string("Simple-ring compatibility mask mismatch: ") + name);
    }
}

std::vector<HoleComponent> inactiveHoleComponents(const BinaryMask& mask) {
    const int width = mask.width();
    const int height = mask.height();
    const BinaryMask exterior = exteriorInactiveCells(mask);
    std::vector<unsigned char> visited(mask.values().size(), 0);
    std::queue<int> queue;
    std::vector<HoleComponent> components;
    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const int start = y0 * width + x0;
            if (mask.get(x0, y0) || exterior.get(x0, y0) ||
                visited[static_cast<std::size_t>(start)]) {
                continue;
            }
            HoleComponent component;
            component.firstIndex = start;
            visited[static_cast<std::size_t>(start)] = 1;
            queue.push(start);
            while (!queue.empty()) {
                const int index = queue.front();
                queue.pop();
                component.cells.push_back(index);
                const int x = index % width;
                const int y = index / width;
                for (const auto& offset : kNeighbors4) {
                    const int nx = x + offset[0];
                    const int ny = y + offset[1];
                    if (!mask.inBounds(nx, ny) || mask.get(nx, ny) ||
                        exterior.get(nx, ny)) {
                        continue;
                    }
                    const int next = ny * width + nx;
                    if (visited[static_cast<std::size_t>(next)]) {
                        continue;
                    }
                    visited[static_cast<std::size_t>(next)] = 1;
                    queue.push(next);
                }
            }
            components.push_back(std::move(component));
        }
    }
    std::sort(components.begin(), components.end(), [](const auto& a, const auto& b) {
        if (a.cells.size() != b.cells.size()) {
            return a.cells.size() > b.cells.size();
        }
        return a.firstIndex < b.firstIndex;
    });
    return components;
}

int totalHoleCells(const std::vector<HoleComponent>& holes) {
    int total = 0;
    for (const HoleComponent& hole : holes) {
        total += static_cast<int>(hole.cells.size());
    }
    return total;
}

int countActiveComponents8(const BinaryMask& mask) {
    const int width = mask.width();
    std::vector<unsigned char> visited(mask.values().size(), 0);
    std::queue<int> queue;
    int count = 0;
    for (int y0 = 0; y0 < mask.height(); ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const int start = y0 * width + x0;
            if (!mask.get(x0, y0) || visited[static_cast<std::size_t>(start)]) {
                continue;
            }
            ++count;
            visited[static_cast<std::size_t>(start)] = 1;
            queue.push(start);
            while (!queue.empty()) {
                const int index = queue.front();
                queue.pop();
                const int x = index % width;
                const int y = index / width;
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const int nx = x + ox;
                        const int ny = y + oy;
                        if (!mask.inBounds(nx, ny) || !mask.get(nx, ny)) {
                            continue;
                        }
                        const int next = ny * width + nx;
                        if (visited[static_cast<std::size_t>(next)]) {
                            continue;
                        }
                        visited[static_cast<std::size_t>(next)] = 1;
                        queue.push(next);
                    }
                }
            }
        }
    }
    return count;
}

double openingCellCost(
    const Grid2D& grid,
    const BinaryMask& attributeMask,
    int index,
    double referenceThreshold,
    const SimpleRingCompatibilityOptions& options,
    const std::vector<double>& retryPenalty) {
    const int x = index % grid.nx();
    const int y = index / grid.nx();
    const double value = grid.value(x, y);
    const double denominator = std::max(1e-12, referenceThreshold);
    const double support = std::isfinite(value)
        ? std::clamp(value / denominator, 0.0, 2.0)
        : 2.0;
    return 1.0 + options.attributeCostWeight * support +
        (attributeMask.get(x, y) ? options.strongAttributePenalty : 0.0) +
        retryPenalty[static_cast<std::size_t>(index)];
}

std::vector<int> minimumCostOpeningPath(
    const Grid2D& grid,
    const BinaryMask& mask,
    const BinaryMask& attributeMask,
    const HoleComponent& hole,
    double referenceThreshold,
    const SimpleRingCompatibilityOptions& options,
    const std::vector<double>& retryPenalty) {
    const int width = mask.width();
    const BinaryMask exterior = exteriorInactiveCells(mask);
    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> distance(mask.values().size(), infinity);
    std::vector<int> parent(mask.values().size(), -1);
    std::priority_queue<
        QueueEntry,
        std::vector<QueueEntry>,
        QueueEntryGreater> queue;

    for (const int source : hole.cells) {
        const int x = source % width;
        const int y = source / width;
        for (const auto& offset : kNeighbors4) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (!mask.inBounds(nx, ny) || !mask.get(nx, ny)) {
                continue;
            }
            const int next = ny * width + nx;
            const double cost = openingCellCost(
                grid,
                attributeMask,
                next,
                referenceThreshold,
                options,
                retryPenalty);
            if (cost < distance[static_cast<std::size_t>(next)]) {
                distance[static_cast<std::size_t>(next)] = cost;
                parent[static_cast<std::size_t>(next)] = source;
                queue.push({cost, next});
            }
        }
    }

    int target = -1;
    while (!queue.empty()) {
        const QueueEntry current = queue.top();
        queue.pop();
        if (current.cost != distance[static_cast<std::size_t>(current.index)]) {
            continue;
        }
        const int x = current.index % width;
        const int y = current.index / width;
        bool touchesExterior = false;
        for (const auto& offset : kNeighbors4) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (mask.inBounds(nx, ny) && exterior.get(nx, ny)) {
                touchesExterior = true;
                break;
            }
        }
        if (touchesExterior) {
            target = current.index;
            break;
        }
        for (const auto& offset : kNeighbors4) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (!mask.inBounds(nx, ny) || !mask.get(nx, ny)) {
                continue;
            }
            const int next = ny * width + nx;
            const double candidateCost = current.cost + openingCellCost(
                grid,
                attributeMask,
                next,
                referenceThreshold,
                options,
                retryPenalty);
            if (candidateCost < distance[static_cast<std::size_t>(next)]) {
                distance[static_cast<std::size_t>(next)] = candidateCost;
                parent[static_cast<std::size_t>(next)] = current.index;
                queue.push({candidateCost, next});
            }
        }
    }
    if (target < 0) {
        return {};
    }

    std::vector<int> path;
    for (int current = target; current >= 0 && mask.values()[
             static_cast<std::size_t>(current)];
         current = parent[static_cast<std::size_t>(current)]) {
        path.push_back(current);
        if (path.size() > static_cast<std::size_t>(options.maximumOpeningCells)) {
            return {};
        }
    }
    std::reverse(path.begin(), path.end());
    return path;
}

bool tryOpenHole(
    const Grid2D& grid,
    const BinaryMask& attributeMask,
    const HoleComponent& hole,
    double referenceThreshold,
    const SimpleRingCompatibilityOptions& options,
    BinaryMask& mask,
    SimpleRingCompatibilityStats& stats) {
    const int activeComponentsBefore = countActiveComponents8(mask);
    const int holesBefore = countInactiveHoles(mask);
    std::vector<double> retryPenalty(mask.values().size(), 0.0);
    for (int attempt = 0; attempt < options.maximumOpeningAttempts; ++attempt) {
        const std::vector<int> path = minimumCostOpeningPath(
            grid,
            mask,
            attributeMask,
            hole,
            referenceThreshold,
            options,
            retryPenalty);
        if (path.empty()) {
            return false;
        }
        BinaryMask trial = mask;
        int attributeCells = 0;
        for (const int index : path) {
            trial.values()[static_cast<std::size_t>(index)] = 0;
            attributeCells += attributeMask.values()[
                static_cast<std::size_t>(index)] ? 1 : 0;
        }
        if (countInactiveHoles(trial) < holesBefore &&
            countActiveComponents8(trial) <= activeComponentsBefore) {
            mask = std::move(trial);
            ++stats.openedHoleCount;
            stats.openingCutCells += static_cast<int>(path.size());
            stats.openingAttributeCells += attributeCells;
            return true;
        }
        for (const int index : path) {
            retryPenalty[static_cast<std::size_t>(index)] += 10000.0;
        }
    }
    return false;
}

} // namespace

void validateSimpleRingCompatibilityOptions(
    const SimpleRingCompatibilityOptions& options) {
    if (options.maximumFillCells < 0 || options.maximumOpeningCells < 1 ||
        options.maximumOpeningAttempts < 1 ||
        !std::isfinite(options.attributeCostWeight) ||
        !std::isfinite(options.strongAttributePenalty) ||
        options.attributeCostWeight < 0.0 ||
        options.strongAttributePenalty < 0.0) {
        throw std::runtime_error(
            "Simple-ring compatibility options are out of range.");
    }
}

SimpleRingCompatibilityResult enforceSimpleRingCompatibility(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const BinaryMask& attributeMask,
    double referenceThreshold,
    const SimpleRingCompatibilityOptions& options) {
    validateSimpleRingCompatibilityOptions(options);
    requireMatchingMask(activeMask, attributeMask, "attribute mask");
    if (activeMask.width() != grid.nx() || activeMask.height() != grid.ny()) {
        throw std::runtime_error(
            "Simple-ring compatibility mask does not match the grid.");
    }
    if (!std::isfinite(referenceThreshold) || referenceThreshold < 0.0) {
        throw std::runtime_error(
            "Simple-ring compatibility reference threshold must be finite and non-negative.");
    }

    SimpleRingCompatibilityResult result{activeMask, {}};
    std::vector<HoleComponent> holes = inactiveHoleComponents(result.mask);
    if (holes.size() > kMaximumHoleComponents) {
        throw std::runtime_error(
            "Simple-ring compatibility stopped because the mask contains too many interior holes.");
    }
    result.stats.initialHoleCount = static_cast<int>(holes.size());
    result.stats.initialHoleCells = totalHoleCells(holes);

    const int activeComponentsBeforeFill = countActiveComponents8(result.mask);
    for (const HoleComponent& hole : holes) {
        if (static_cast<int>(hole.cells.size()) > options.maximumFillCells) {
            continue;
        }
        BinaryMask trial = result.mask;
        for (const int index : hole.cells) {
            trial.values()[static_cast<std::size_t>(index)] = 1;
        }
        if (countActiveComponents8(trial) != activeComponentsBeforeFill) {
            ++result.stats.fillRejectedComponents;
            continue;
        }
        result.mask = std::move(trial);
        ++result.stats.filledHoleCount;
        result.stats.filledCells += static_cast<int>(hole.cells.size());
    }

    holes = inactiveHoleComponents(result.mask);
    while (!holes.empty()) {
        const HoleComponent hole = holes.front();
        if (!tryOpenHole(
                grid,
                attributeMask,
                hole,
                referenceThreshold,
                options,
                result.mask,
                result.stats)) {
            ++result.stats.openingRejectedComponents;
            break;
        }
        holes = inactiveHoleComponents(result.mask);
    }

    holes = inactiveHoleComponents(result.mask);
    result.stats.remainingHoleCount = static_cast<int>(holes.size());
    result.stats.remainingHoleCells = totalHoleCells(holes);
    return result;
}

} // namespace fpt
