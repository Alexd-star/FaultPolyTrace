#include "WeakPathSupport.h"
#include "SkeletonGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fpt {
namespace {

int indexOf(int width, int x, int y) {
    return y * width + x;
}

bool isSupported(double value, double supportThreshold) {
    return std::isfinite(value) && value >= supportThreshold;
}

double angleBetweenDegrees(double ax, double ay, double bx, double by) {
    const double normA = std::sqrt(ax * ax + ay * ay);
    const double normB = std::sqrt(bx * bx + by * by);
    if (normA <= 1e-12 || normB <= 1e-12) {
        return 0.0;
    }
    double cosine = (ax * bx + ay * by) / (normA * normB);
    cosine = std::clamp(cosine, -1.0, 1.0);
    return std::acos(cosine) * 180.0 / 3.14159265358979323846;
}

double quantile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    const double clamped = std::clamp(fraction, 0.0, 1.0);
    const double position = clamped * static_cast<double>(values.size() - 1);
    const std::size_t lower = static_cast<std::size_t>(std::floor(position));
    const std::size_t upper = static_cast<std::size_t>(std::ceil(position));
    const double weight = position - static_cast<double>(lower);
    return values[lower] * (1.0 - weight) + values[upper] * weight;
}

void computeShapeStats(
    const Grid2D& grid,
    const std::vector<std::pair<int, int>>& cells,
    WeakPathSupportStats& stats) {
    if (cells.empty()) {
        stats.directionX = 1.0;
        stats.directionY = 0.0;
        stats.majorAxisLength = 0.0;
        stats.estimatedWidth = 0.0;
        stats.aspectRatio = 1.0;
        return;
    }

    double meanX = 0.0;
    double meanY = 0.0;
    for (const auto& cell : cells) {
        meanX += grid.worldX(static_cast<double>(cell.first));
        meanY += grid.worldY(static_cast<double>(cell.second));
    }
    meanX /= static_cast<double>(cells.size());
    meanY /= static_cast<double>(cells.size());

    double covarianceXX = 0.0;
    double covarianceYY = 0.0;
    double covarianceXY = 0.0;
    for (const auto& cell : cells) {
        const double dx =
            grid.worldX(static_cast<double>(cell.first)) - meanX;
        const double dy =
            grid.worldY(static_cast<double>(cell.second)) - meanY;
        covarianceXX += dx * dx;
        covarianceYY += dy * dy;
        covarianceXY += dx * dy;
    }

    const double angle = 0.5 * std::atan2(
        2.0 * covarianceXY,
        covarianceXX - covarianceYY);
    stats.directionX = std::cos(angle);
    stats.directionY = std::sin(angle);

    double minProjection = std::numeric_limits<double>::infinity();
    double maxProjection = -std::numeric_limits<double>::infinity();
    for (const auto& cell : cells) {
        const double dx =
            grid.worldX(static_cast<double>(cell.first)) - meanX;
        const double dy =
            grid.worldY(static_cast<double>(cell.second)) - meanY;
        const double projection = dx * stats.directionX + dy * stats.directionY;
        minProjection = std::min(minProjection, projection);
        maxProjection = std::max(maxProjection, projection);
    }

    const double minSpacing = std::min(grid.dx(), grid.dy());
    stats.majorAxisLength =
        std::max(minSpacing, maxProjection - minProjection + minSpacing);
    const double physicalArea =
        static_cast<double>(cells.size()) * grid.dx() * grid.dy();
    const double physicalWidth = physicalArea / stats.majorAxisLength;
    stats.estimatedWidth = physicalWidth / minSpacing;
    stats.aspectRatio = stats.majorAxisLength /
        std::max(physicalWidth, 1e-12);
}

struct ComponentEvidence {
    double meanAttribute = 0.0;
    double lowerQuantileAttribute = 0.0;
    double estimatedWidth = 0.0;
    double aspectRatio = 1.0;
    double maxTurnAngleDegrees = 0.0;
};

struct ComponentLabels {
    std::vector<int> labels;
    std::vector<int> cellCounts;
};

constexpr int kNeighborOffsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1}
};

ComponentLabels labelComponents(const BinaryMask& mask) {
    const int width = mask.width();
    const int height = mask.height();
    ComponentLabels result;
    result.labels.assign(static_cast<std::size_t>(width * height), -1);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t startPosition =
                static_cast<std::size_t>(indexOf(width, x, y));
            if (!mask.get(x, y) || result.labels[startPosition] >= 0) {
                continue;
            }

            const int componentId = static_cast<int>(result.cellCounts.size());
            int cellCount = 0;
            std::queue<std::pair<int, int>> queue;
            queue.push({x, y});
            result.labels[startPosition] = componentId;
            while (!queue.empty()) {
                const auto current = queue.front();
                queue.pop();
                ++cellCount;
                for (const auto& offset : kNeighborOffsets) {
                    const int nextX = current.first + offset[0];
                    const int nextY = current.second + offset[1];
                    if (!mask.inBounds(nextX, nextY) || !mask.get(nextX, nextY)) {
                        continue;
                    }
                    const std::size_t nextPosition =
                        static_cast<std::size_t>(indexOf(width, nextX, nextY));
                    if (result.labels[nextPosition] >= 0) {
                        continue;
                    }
                    result.labels[nextPosition] = componentId;
                    queue.push({nextX, nextY});
                }
            }
            result.cellCounts.push_back(cellCount);
        }
    }
    return result;
}

ComponentEvidence evaluateComponentEvidence(
    const Grid2D& grid,
    const std::vector<std::pair<int, int>>& cells,
    const WeakPathSupportOptions& options) {
    ComponentEvidence evidence;
    if (cells.empty()) {
        return evidence;
    }

    int minX = cells.front().first;
    int maxX = cells.front().first;
    int minY = cells.front().second;
    int maxY = cells.front().second;
    std::vector<double> attributes;
    attributes.reserve(cells.size());
    double sum = 0.0;
    for (const auto& cell : cells) {
        minX = std::min(minX, cell.first);
        maxX = std::max(maxX, cell.first);
        minY = std::min(minY, cell.second);
        maxY = std::max(maxY, cell.second);
        const double value = grid.value(cell.first, cell.second);
        if (std::isfinite(value)) {
            attributes.push_back(value);
            sum += value;
        }
    }
    evidence.meanAttribute = attributes.empty()
        ? 0.0
        : sum / static_cast<double>(attributes.size());
    evidence.lowerQuantileAttribute =
        quantile(attributes, options.lowerQuantile);

    const int padding = 1;
    const int localWidth = maxX - minX + 1 + 2 * padding;
    const int localHeight = maxY - minY + 1 + 2 * padding;
    BinaryMask localMask(localWidth, localHeight);
    for (const auto& cell : cells) {
        localMask.set(
            cell.first - minX + padding,
            cell.second - minY + padding,
            true);
    }

    const BinaryMask skeleton = skeletonizeZhangSuen(localMask);

    const SkeletonGraph graph = buildSkeletonGraph(skeleton);
    double skeletonLengthCells = 0.0;
    for (const SkeletonBranch& branch : graph.branches) {
        for (std::size_t pointIndex = 1;
             pointIndex < branch.points.size();
             ++pointIndex) {
            const SkeletonPoint& previous =
                branch.points[pointIndex - 1];
            const SkeletonPoint& current =
                branch.points[pointIndex];
            skeletonLengthCells += std::hypot(
                static_cast<double>(current.x - previous.x),
                static_cast<double>(current.y - previous.y));
        }
    }
    skeletonLengthCells = std::max(skeletonLengthCells, 1.0);
    evidence.estimatedWidth =
        static_cast<double>(cells.size()) / skeletonLengthCells;
    evidence.aspectRatio =
        skeletonLengthCells / std::max(evidence.estimatedWidth, 1e-12);

    const int requestedWindow = std::max(1, options.directionWindow);
    for (const SkeletonBranch& branch : graph.branches) {
        const int pointCount = static_cast<int>(branch.points.size());
        const int window = std::min(requestedWindow, (pointCount - 1) / 2);
        if (window < 1) {
            continue;
        }
        for (int i = window; i + window < pointCount; ++i) {
            const SkeletonPoint& previous =
                branch.points[static_cast<std::size_t>(i - window)];
            const SkeletonPoint& current =
                branch.points[static_cast<std::size_t>(i)];
            const SkeletonPoint& next =
                branch.points[static_cast<std::size_t>(i + window)];
            const double incomingX =
                static_cast<double>(current.x - previous.x) * grid.dx();
            const double incomingY =
                static_cast<double>(current.y - previous.y) * grid.dy();
            const double outgoingX =
                static_cast<double>(next.x - current.x) * grid.dx();
            const double outgoingY =
                static_cast<double>(next.y - current.y) * grid.dy();
            evidence.maxTurnAngleDegrees = std::max(
                evidence.maxTurnAngleDegrees,
                angleBetweenDegrees(
                    incomingX,
                    incomingY,
                    outgoingX,
                    outgoingY));
        }
    }
    return evidence;
}

void applyComponentEvidenceGates(
    const Grid2D& grid,
    const WeakPathSupportOptions& options,
    BinaryMask& acceptedMask,
    WeakPathSupportStats& stats) {
    const bool evidenceEnabled =
        options.minEvidenceCells > 0 &&
        (std::isfinite(options.minMeanAttribute) ||
         std::isfinite(options.minLowerQuantileAttribute) ||
         options.minAspectRatio > 0.0 ||
         std::isfinite(options.maxEstimatedWidth) ||
         options.maxTurnAngleDegrees < 180.0);
    if (acceptedMask.empty()) {
        return;
    }

    const int width = acceptedMask.width();
    const int height = acceptedMask.height();
    std::vector<unsigned char> visited(
        static_cast<std::size_t>(width * height),
        0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t startPosition =
                static_cast<std::size_t>(indexOf(width, x, y));
            if (!acceptedMask.get(x, y) || visited[startPosition]) {
                continue;
            }

            std::vector<std::pair<int, int>> cells;
            std::queue<std::pair<int, int>> queue;
            queue.push({x, y});
            visited[startPosition] = 1;
            while (!queue.empty()) {
                const auto current = queue.front();
                queue.pop();
                cells.push_back(current);
                for (const auto& offset : kNeighborOffsets) {
                    const int nextX = current.first + offset[0];
                    const int nextY = current.second + offset[1];
                    if (!acceptedMask.inBounds(nextX, nextY)) {
                        continue;
                    }
                    const std::size_t nextPosition =
                        static_cast<std::size_t>(indexOf(width, nextX, nextY));
                    if (visited[nextPosition] ||
                        !acceptedMask.get(nextX, nextY)) {
                        continue;
                    }
                    visited[nextPosition] = 1;
                    queue.push({nextX, nextY});
                }
            }

            bool attributeRejected = false;
            bool shapeRejected = false;
            bool directionRejected = false;
            if (evidenceEnabled &&
                static_cast<int>(cells.size()) >= options.minEvidenceCells) {
                ++stats.evidenceEvaluatedFragmentCount;
                const ComponentEvidence evidence =
                    evaluateComponentEvidence(grid, cells, options);
                stats.maxObservedComponentWidth = std::max(
                    stats.maxObservedComponentWidth,
                    evidence.estimatedWidth);
                stats.maxObservedTurnAngleDegrees = std::max(
                    stats.maxObservedTurnAngleDegrees,
                    evidence.maxTurnAngleDegrees);
                attributeRejected =
                    evidence.meanAttribute < options.minMeanAttribute ||
                    evidence.lowerQuantileAttribute <
                        options.minLowerQuantileAttribute;
                shapeRejected =
                    evidence.aspectRatio < options.minAspectRatio ||
                    evidence.estimatedWidth > options.maxEstimatedWidth;
                directionRejected =
                    evidence.maxTurnAngleDegrees >
                        options.maxTurnAngleDegrees;
            }

            if (!attributeRejected && !shapeRejected && !directionRejected) {
                continue;
            }

            if (attributeRejected) {
                ++stats.attributeRejectedFragmentCount;
            }
            if (shapeRejected) {
                ++stats.shapeRejectedFragmentCount;
            }
            if (directionRejected) {
                ++stats.directionRejectedFragmentCount;
            }
            for (const auto& cell : cells) {
                acceptedMask.set(cell.first, cell.second, false);
            }
        }
    }
}

void summarizeCandidateComponents(
    const ComponentLabels& candidateComponents,
    const BinaryMask& acceptedMask,
    WeakPathSupportStats& stats) {
    stats.candidateComponentCount =
        static_cast<int>(candidateComponents.cellCounts.size());
    stats.acceptedComponentCount = 0;
    stats.rejectedComponentCount = 0;
    stats.partiallyRejectedComponentCount = 0;
    std::vector<int> acceptedCounts(
        candidateComponents.cellCounts.size(),
        0);
    for (int y = 0; y < acceptedMask.height(); ++y) {
        for (int x = 0; x < acceptedMask.width(); ++x) {
            if (!acceptedMask.get(x, y)) {
                continue;
            }
            const int componentId = candidateComponents.labels[
                static_cast<std::size_t>(
                    indexOf(acceptedMask.width(), x, y))];
            if (componentId >= 0) {
                ++acceptedCounts[static_cast<std::size_t>(componentId)];
            }
        }
    }
    for (std::size_t componentId = 0;
         componentId < candidateComponents.cellCounts.size();
         ++componentId) {
        const int accepted = acceptedCounts[componentId];
        const int total = candidateComponents.cellCounts[componentId];
        if (accepted <= 0) {
            ++stats.rejectedComponentCount;
            continue;
        }
        ++stats.acceptedComponentCount;
        if (accepted < total) {
            ++stats.partiallyRejectedComponentCount;
        }
    }
}

void removeAcceptedCellsWithoutValidSeedPath(
    const Grid2D& grid,
    const BinaryMask& seedMask,
    int seedSearchRadius,
    double supportThreshold,
    int maxUnsupportedRun,
    BinaryMask& acceptedMask,
    WeakPathSupportStats& stats) {
    const int width = acceptedMask.width();
    const int height = acceptedMask.height();
    const int unreachable = std::numeric_limits<int>::max();
    std::vector<int> bestUnsupportedRun(
        static_cast<std::size_t>(width * height),
        unreachable);
    std::queue<std::pair<int, int>> queue;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!acceptedMask.get(x, y)) {
                continue;
            }
            bool touchesSeed = seedMask.get(x, y);
            if (!touchesSeed && seedSearchRadius == 1) {
                for (const auto& offset : kNeighborOffsets) {
                    if (seedMask.get(x + offset[0], y + offset[1])) {
                        touchesSeed = true;
                        break;
                    }
                }
            }
            if (!touchesSeed) {
                continue;
            }
            const int unsupportedRun =
                isSupported(
                    grid.value(x, y),
                    supportThreshold)
                ? 0
                : 1;
            if (unsupportedRun > maxUnsupportedRun) {
                continue;
            }
            const std::size_t position =
                static_cast<std::size_t>(indexOf(width, x, y));
            bestUnsupportedRun[position] = unsupportedRun;
            queue.push({x, y});
        }
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();
        const int currentRun = bestUnsupportedRun[
            static_cast<std::size_t>(
                indexOf(width, current.first, current.second))];
        for (const auto& offset : kNeighborOffsets) {
            const int nextX = current.first + offset[0];
            const int nextY = current.second + offset[1];
            if (!acceptedMask.inBounds(nextX, nextY) ||
                !acceptedMask.get(nextX, nextY)) {
                continue;
            }
            const std::size_t nextPosition =
                static_cast<std::size_t>(indexOf(width, nextX, nextY));
            const int nextRun =
                isSupported(
                    grid.value(nextX, nextY),
                    supportThreshold)
                ? 0
                : currentRun + 1;
            if (nextRun > maxUnsupportedRun ||
                nextRun >= bestUnsupportedRun[nextPosition]) {
                continue;
            }
            bestUnsupportedRun[nextPosition] = nextRun;
            queue.push({nextX, nextY});
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t position =
                static_cast<std::size_t>(indexOf(width, x, y));
            if (!acceptedMask.get(x, y) ||
                bestUnsupportedRun[position] != unreachable) {
                continue;
            }
            acceptedMask.set(x, y, false);
            ++stats.rootConflictRejectedCellCount;
        }
    }
}

} // namespace

WeakPathSupportResult selectWeakPathSupport(
    const Grid2D& grid,
    const BinaryMask& candidateMask,
    const BinaryMask& seedMask,
    const WeakPathSupportOptions& options) {
    if (grid.empty()) {
        throw std::runtime_error("Weak path support requires a non-empty grid.");
    }
    if (candidateMask.width() != grid.nx() || candidateMask.height() != grid.ny()) {
        throw std::runtime_error("Weak path candidate mask dimensions do not match the grid.");
    }
    if (seedMask.width() != grid.nx() || seedMask.height() != grid.ny()) {
        throw std::runtime_error("Weak path seed mask dimensions do not match the grid.");
    }
    if (!std::isfinite(options.supportThreshold)) {
        throw std::runtime_error("Weak path support threshold must be finite.");
    }
    if (options.maxUnsupportedRun < 0) {
        throw std::runtime_error("Weak path maxUnsupportedRun must be non-negative.");
    }
    if (options.seedSearchRadius < 0 || options.seedSearchRadius > 1) {
        throw std::runtime_error("Weak path seedSearchRadius must be 0 or 1.");
    }
    if (options.minEvidenceCells < 0) {
        throw std::runtime_error("Weak path minEvidenceCells must be non-negative.");
    }
    if (std::isnan(options.minMeanAttribute) ||
        std::isnan(options.minLowerQuantileAttribute)) {
        throw std::runtime_error("Weak path attribute evidence thresholds must not be NaN.");
    }
    if (!std::isfinite(options.lowerQuantile) ||
        options.lowerQuantile < 0.0 ||
        options.lowerQuantile > 1.0) {
        throw std::runtime_error("Weak path lowerQuantile must be between 0 and 1.");
    }
    if (!std::isfinite(options.minAspectRatio) ||
        options.minAspectRatio < 0.0) {
        throw std::runtime_error("Weak path minAspectRatio must be non-negative.");
    }
    if (std::isnan(options.maxEstimatedWidth) ||
        options.maxEstimatedWidth <= 0.0) {
        throw std::runtime_error("Weak path maxEstimatedWidth must be positive.");
    }
    if (!std::isfinite(options.maxTurnAngleDegrees) ||
        options.maxTurnAngleDegrees < 0.0 ||
        options.maxTurnAngleDegrees > 180.0) {
        throw std::runtime_error("Weak path maxTurnAngleDegrees must be between 0 and 180.");
    }
    if (options.directionWindow < 1) {
        throw std::runtime_error("Weak path directionWindow must be positive.");
    }

    WeakPathSupportResult result{
        BinaryMask(grid.nx(), grid.ny()),
        WeakPathSupportStats{}};
    BinaryMask finiteCandidateMask(grid.nx(), grid.ny());
    std::vector<std::pair<int, int>> candidateCells;
    candidateCells.reserve(static_cast<std::size_t>(candidateMask.countActive()));
    double attributeSum = 0.0;
    int finiteAttributeCount = 0;

    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (!candidateMask.get(x, y)) {
                continue;
            }
            const double value = grid.value(x, y);
            if (!std::isfinite(value)) {
                continue;
            }
            finiteCandidateMask.set(x, y, true);
            candidateCells.emplace_back(x, y);
            attributeSum += value;
            ++finiteAttributeCount;
        }
    }

    result.stats.candidateCellCount = static_cast<int>(candidateCells.size());
    result.stats.meanAttribute = finiteAttributeCount > 0
        ? attributeSum / static_cast<double>(finiteAttributeCount)
        : 0.0;
    computeShapeStats(grid, candidateCells, result.stats);
    const int width = grid.nx();
    const int unreachable = std::numeric_limits<int>::max();
    const ComponentLabels candidateComponents =
        labelComponents(finiteCandidateMask);
    const ComponentLabels seedComponents =
        labelComponents(seedMask);
    result.stats.seedCellCount = seedMask.countActive();
    std::vector<int> bestUnsupportedRun(
        static_cast<std::size_t>(grid.nx() * grid.ny()),
        unreachable);
    std::vector<int> bestDistance(
        static_cast<std::size_t>(grid.nx() * grid.ny()),
        unreachable);
    std::vector<int> ownerRoot(
        static_cast<std::size_t>(grid.nx() * grid.ny()),
        -1);
    std::queue<std::pair<int, int>> queue;

    for (const auto& cell : candidateCells) {
        const int x = cell.first;
        const int y = cell.second;
        const std::size_t position =
            static_cast<std::size_t>(indexOf(width, x, y));
        int rootId = seedComponents.labels[position];
        int seedDistance = rootId >= 0 ? 0 : unreachable;
        if (options.seedSearchRadius > 0) {
            for (const auto& offset : kNeighborOffsets) {
                const int seedX = x + offset[0];
                const int seedY = y + offset[1];
                if (!seedMask.inBounds(seedX, seedY) ||
                    !seedMask.get(seedX, seedY)) {
                    continue;
                }
                const int adjacentRoot = seedComponents.labels[
                    static_cast<std::size_t>(
                        indexOf(width, seedX, seedY))];
                if (adjacentRoot < 0) {
                    continue;
                }
                if (seedDistance > 1 ||
                    (seedDistance == 1 && adjacentRoot < rootId)) {
                    rootId = adjacentRoot;
                    seedDistance = 1;
                }
            }
        }
        if (rootId < 0 || seedDistance == unreachable) {
            continue;
        }
        ++result.stats.seedContactCellCount;
        const int unsupportedRun =
            isSupported(grid.value(x, y), options.supportThreshold)
            ? 0
            : 1;
        if (unsupportedRun > options.maxUnsupportedRun) {
            continue;
        }
        bestUnsupportedRun[position] = unsupportedRun;
        bestDistance[position] = seedDistance;
        ownerRoot[position] = rootId;
        result.acceptedMask.set(x, y, true);
        queue.push(cell);
    }

    if (result.stats.seedContactCellCount == 0) {
        result.stats.insufficientSeedEvidence = true;
        result.stats.rejectedCellCount = result.stats.candidateCellCount;
        summarizeCandidateComponents(
            candidateComponents,
            result.acceptedMask,
            result.stats);
        return result;
    }

    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop();
        const int currentRun = bestUnsupportedRun[
            static_cast<std::size_t>(indexOf(width, current.first, current.second))];
        const int currentDistance = bestDistance[
            static_cast<std::size_t>(
                indexOf(width, current.first, current.second))];
        const int currentRoot = ownerRoot[
            static_cast<std::size_t>(
                indexOf(width, current.first, current.second))];
        for (const auto& offset : kNeighborOffsets) {
            const int nextX = current.first + offset[0];
            const int nextY = current.second + offset[1];
            if (!finiteCandidateMask.get(nextX, nextY)) {
                continue;
            }
            const int nextRun = isSupported(
                grid.value(nextX, nextY),
                options.supportThreshold)
                ? 0
                : currentRun + 1;
            if (nextRun > options.maxUnsupportedRun) {
                continue;
            }
            const std::size_t nextPosition =
                static_cast<std::size_t>(indexOf(width, nextX, nextY));
            const int nextDistance = currentDistance + 1;
            const bool improves =
                nextRun < bestUnsupportedRun[nextPosition] ||
                (nextRun == bestUnsupportedRun[nextPosition] &&
                 nextDistance < bestDistance[nextPosition]) ||
                (nextRun == bestUnsupportedRun[nextPosition] &&
                 nextDistance == bestDistance[nextPosition] &&
                 currentRoot < ownerRoot[nextPosition]);
            if (!improves) {
                continue;
            }
            bestUnsupportedRun[nextPosition] = nextRun;
            bestDistance[nextPosition] = nextDistance;
            ownerRoot[nextPosition] = currentRoot;
            result.acceptedMask.set(nextX, nextY, true);
            queue.push({nextX, nextY});
        }
    }

    std::vector<unsigned char> rootConflict(
        static_cast<std::size_t>(grid.nx() * grid.ny()),
        0);
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (!result.acceptedMask.get(x, y) ||
                isSupported(grid.value(x, y), options.supportThreshold)) {
                continue;
            }
            const std::size_t position =
                static_cast<std::size_t>(indexOf(width, x, y));
            for (const auto& offset : kNeighborOffsets) {
                const int nextX = x + offset[0];
                const int nextY = y + offset[1];
                if (!result.acceptedMask.inBounds(nextX, nextY) ||
                    !result.acceptedMask.get(nextX, nextY) ||
                    isSupported(
                        grid.value(nextX, nextY),
                        options.supportThreshold)) {
                    continue;
                }
                const std::size_t nextPosition =
                    static_cast<std::size_t>(
                        indexOf(width, nextX, nextY));
                if (nextPosition <= position ||
                    ownerRoot[position] < 0 ||
                    ownerRoot[nextPosition] < 0 ||
                    ownerRoot[position] == ownerRoot[nextPosition] ||
                    bestUnsupportedRun[position] +
                        bestUnsupportedRun[nextPosition] <=
                        options.maxUnsupportedRun) {
                    continue;
                }
                const double value = grid.value(x, y);
                const double nextValue = grid.value(nextX, nextY);
                std::size_t cutPosition = nextPosition;
                if (value < nextValue ||
                    (value == nextValue &&
                     bestUnsupportedRun[position] >
                         bestUnsupportedRun[nextPosition])) {
                    cutPosition = position;
                } else if (
                    value == nextValue &&
                    bestUnsupportedRun[position] ==
                        bestUnsupportedRun[nextPosition]) {
                    cutPosition = std::max(position, nextPosition);
                }
                rootConflict[cutPosition] = 1;
            }
        }
    }
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            const std::size_t position =
                static_cast<std::size_t>(indexOf(width, x, y));
            if (!rootConflict[position]) {
                continue;
            }
            result.acceptedMask.set(x, y, false);
            ++result.stats.rootConflictRejectedCellCount;
        }
    }
    removeAcceptedCellsWithoutValidSeedPath(
        grid,
        seedMask,
        options.seedSearchRadius,
        options.supportThreshold,
        options.maxUnsupportedRun,
        result.acceptedMask,
        result.stats);

    applyComponentEvidenceGates(
        grid,
        options,
        result.acceptedMask,
        result.stats);
    summarizeCandidateComponents(
        candidateComponents,
        result.acceptedMask,
        result.stats);
    result.stats.acceptedCellCount = result.acceptedMask.countActive();
    result.stats.rejectedCellCount =
        result.stats.candidateCellCount - result.stats.acceptedCellCount;
    std::vector<std::pair<int, int>> acceptedCells;
    acceptedCells.reserve(
        static_cast<std::size_t>(result.stats.acceptedCellCount));
    std::vector<double> acceptedAttributes;
    acceptedAttributes.reserve(
        static_cast<std::size_t>(result.stats.acceptedCellCount));
    double acceptedAttributeSum = 0.0;
    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            if (!result.acceptedMask.get(x, y)) {
                continue;
            }
            acceptedCells.push_back({x, y});
            const double value = grid.value(x, y);
            if (std::isfinite(value)) {
                acceptedAttributes.push_back(value);
                acceptedAttributeSum += value;
            }
            const int unsupportedRun = bestUnsupportedRun[
                static_cast<std::size_t>(indexOf(width, x, y))];
            if (unsupportedRun != unreachable) {
                result.stats.maxAcceptedUnsupportedRun = std::max(
                    result.stats.maxAcceptedUnsupportedRun,
                    unsupportedRun);
            }
        }
    }
    if (!acceptedAttributes.empty()) {
        result.stats.meanAttribute =
            acceptedAttributeSum /
            static_cast<double>(acceptedAttributes.size());
        result.stats.lowerQuantileAttribute =
            quantile(acceptedAttributes, options.lowerQuantile);
    } else {
        result.stats.meanAttribute = 0.0;
        result.stats.lowerQuantileAttribute = 0.0;
    }
    computeShapeStats(grid, acceptedCells, result.stats);
    return result;
}

} // namespace fpt
