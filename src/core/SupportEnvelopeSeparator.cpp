#include "SupportEnvelopeSeparator.h"

#include "GridMetric.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>
#include <limits>
#include <vector>

namespace fpt {
namespace {

struct Cell {
    int x = 0;
    int y = 0;
};

constexpr int kOffsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1},
    {-1, 0},           {1, 0},
    {-1, 1},  {0, 1},  {1, 1}
};

double physicalAspect(
    int count,
    double sumX,
    double sumY,
    double sumXX,
    double sumXY,
    double sumYY,
    double minimumSpacing) {
    if (count <= 1) {
        return 1.0;
    }
    const double inverseCount = 1.0 / static_cast<double>(count);
    const double meanX = sumX * inverseCount;
    const double meanY = sumY * inverseCount;
    const double covarianceXX = std::max(
        0.0, sumXX * inverseCount - meanX * meanX);
    const double covarianceXY =
        sumXY * inverseCount - meanX * meanY;
    const double covarianceYY = std::max(
        0.0, sumYY * inverseCount - meanY * meanY);
    const double trace = covarianceXX + covarianceYY;
    const double discriminant = std::sqrt(std::max(
        0.0,
        (covarianceXX - covarianceYY) *
            (covarianceXX - covarianceYY) +
        4.0 * covarianceXY * covarianceXY));
    const double major = std::max(0.0, 0.5 * (trace + discriminant));
    const double minor = std::max(0.0, 0.5 * (trace - discriminant));
    const double samplingVariance =
        minimumSpacing * minimumSpacing / 12.0;
    return std::sqrt(
        (major + samplingVariance) /
        (minor + samplingVariance));
}

void requireMatchingDimensions(
    const Grid2D& grid,
    const BinaryMask& mask,
    const char* name) {
    if (mask.width() != grid.nx() || mask.height() != grid.ny()) {
        throw std::runtime_error(std::string(name) +
            " dimensions do not match the grid.");
    }
}

} // namespace

void validateSupportEnvelopeSeparatorOptions(
    const SupportEnvelopeSeparatorOptions& options) {
    if (!std::isfinite(options.lowThreshold) ||
        options.lowThreshold < 0.0) {
        throw std::runtime_error(
            "Support-envelope low threshold must be finite and non-negative.");
    }
    if (!std::isfinite(options.supportRadiusWorld) ||
        options.supportRadiusWorld < 0.0) {
        throw std::runtime_error(
            "Support-envelope radius must be finite and non-negative.");
    }
    if (options.minPartCells < 1 || options.minRemovedCells < 1) {
        throw std::runtime_error(
            "Support-envelope cell limits must be positive.");
    }
    if (!std::isfinite(options.minPartRatio) ||
        options.minPartRatio < 0.0 || options.minPartRatio > 0.5) {
        throw std::runtime_error(
            "Support-envelope minimum part ratio must be in [0, 0.5].");
    }
    if (!std::isfinite(options.minLowRatio) ||
        options.minLowRatio < 0.0 || options.minLowRatio > 1.0) {
        throw std::runtime_error(
            "Support-envelope minimum low-value ratio must be in [0, 1].");
    }
    if (!std::isfinite(options.maxRemovedRatio) ||
        options.maxRemovedRatio <= 0.0 || options.maxRemovedRatio > 0.5) {
        throw std::runtime_error(
            "Support-envelope maximum removal ratio must be in (0, 0.5].");
    }
    if (!std::isfinite(options.minResultingPartPhysicalAspect) ||
        options.minResultingPartPhysicalAspect < 1.0) {
        throw std::runtime_error(
            "Support-envelope part aspect must be finite and at least one.");
    }
    if (options.maxEvidenceRecords < 0) {
        throw std::runtime_error(
            "Support-envelope evidence record limit must be non-negative.");
    }
}

SupportEnvelopeSeparatorResult separateUnsupportedEnvelopes(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const SupportEnvelopeSeparatorOptions& options,
    const BinaryMask* protectedMask) {
    validateSupportEnvelopeSeparatorOptions(options);
    requireMatchingDimensions(grid, activeMask, "Active mask");
    if (protectedMask != nullptr) {
        requireMatchingDimensions(grid, *protectedMask, "Protected mask");
    }

    SupportEnvelopeSeparatorResult result;
    result.committedCutMask = BinaryMask(grid.nx(), grid.ny());
    result.candidateCutMask = BinaryMask(grid.nx(), grid.ny());
    if (!options.enabled || options.supportRadiusWorld <= 0.0 ||
        activeMask.empty()) {
        return result;
    }

    const int width = grid.nx();
    const int height = grid.ny();
    const auto indexOf = [width](int x, int y) {
        return y * width + x;
    };
    const auto isLowValue = [&grid, &options](int x, int y) {
        const double value = grid.value(x, y);
        return !std::isfinite(value) || value <= options.lowThreshold;
    };
    const auto isProtected = [protectedMask](int x, int y) {
        return protectedMask != nullptr && protectedMask->get(x, y);
    };

    const GridMetric metric(grid);
    const std::vector<GridOffset> supportOffsets =
        metric.offsetsWithinRadius(options.supportRadiusWorld);
    std::vector<unsigned char> visited(
        static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> inComponent(
        static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> supportVisited(
        static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> splitVisited(
        static_cast<std::size_t>(width * height), 0);
    std::vector<unsigned char> cutCell(
        static_cast<std::size_t>(width * height), 0);

    int componentId = 0;
    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const std::size_t startIndex = static_cast<std::size_t>(
                indexOf(x0, y0));
            if (!activeMask.get(x0, y0) || visited[startIndex]) {
                continue;
            }

            ++componentId;
            ++result.stats.evaluatedComponents;
            SupportEnvelopeDecision evidence;
            evidence.componentId = componentId;
            std::vector<Cell> cells;
            std::queue<Cell> queue;
            queue.push({x0, y0});
            visited[startIndex] = 1;
            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                cells.push_back(cell);
                inComponent[static_cast<std::size_t>(
                    indexOf(cell.x, cell.y))] = 1;
                if (isLowValue(cell.x, cell.y)) {
                    ++evidence.lowCells;
                }
                for (const auto& offset : kOffsets) {
                    const int x = cell.x + offset[0];
                    const int y = cell.y + offset[1];
                    if (!grid.inBounds(x, y)) {
                        continue;
                    }
                    const std::size_t index = static_cast<std::size_t>(
                        indexOf(x, y));
                    if (!activeMask.get(x, y) || visited[index]) {
                        continue;
                    }
                    visited[index] = 1;
                    queue.push({x, y});
                }
            }

            evidence.componentCells = static_cast<int>(cells.size());
            evidence.lowRatio = evidence.componentCells > 0
                ? static_cast<double>(evidence.lowCells) /
                    static_cast<double>(evidence.componentCells)
                : 0.0;
            const double minimumMajorPartCells =
                static_cast<double>(evidence.componentCells) *
                options.minPartRatio;

            for (const Cell& start : cells) {
                const std::size_t index = static_cast<std::size_t>(
                    indexOf(start.x, start.y));
                if (supportVisited[index] || isLowValue(start.x, start.y)) {
                    continue;
                }
                ++evidence.observedSupportComponents;
                int supportCells = 0;
                std::queue<Cell> supportQueue;
                supportQueue.push(start);
                supportVisited[index] = 1;
                while (!supportQueue.empty()) {
                    const Cell cell = supportQueue.front();
                    supportQueue.pop();
                    ++supportCells;
                    for (const auto& offset : kOffsets) {
                        const int x = cell.x + offset[0];
                        const int y = cell.y + offset[1];
                        if (!grid.inBounds(x, y)) {
                            continue;
                        }
                        const std::size_t next = static_cast<std::size_t>(
                            indexOf(x, y));
                        if (!inComponent[next] || supportVisited[next] ||
                            isLowValue(x, y)) {
                            continue;
                        }
                        supportVisited[next] = 1;
                        supportQueue.push({x, y});
                    }
                }
                if (supportCells >= options.minPartCells &&
                    static_cast<double>(supportCells) >=
                        minimumMajorPartCells) {
                    ++evidence.observedMajorSupportComponents;
                }
            }

            std::vector<Cell> candidateCuts;
            const bool largeEnough = evidence.componentCells >=
                options.minPartCells * 2 + options.minRemovedCells;
            if (largeEnough && evidence.lowRatio >= options.minLowRatio) {
                candidateCuts.reserve(static_cast<std::size_t>(
                    evidence.lowCells));
                for (const Cell& cell : cells) {
                    if (!isLowValue(cell.x, cell.y)) {
                        continue;
                    }
                    if (isProtected(cell.x, cell.y)) {
                        ++result.stats.protectedCells;
                        continue;
                    }
                    bool nearObservedSupport = false;
                    for (const GridOffset& offset : supportOffsets) {
                        const int x = cell.x + offset.x;
                        const int y = cell.y + offset.y;
                        if (!grid.inBounds(x, y)) {
                            continue;
                        }
                        const std::size_t nearby = static_cast<std::size_t>(
                            indexOf(x, y));
                        if (inComponent[nearby] && !isLowValue(x, y)) {
                            nearObservedSupport = true;
                            break;
                        }
                    }
                    if (!nearObservedSupport) {
                        candidateCuts.push_back(cell);
                        cutCell[static_cast<std::size_t>(
                            indexOf(cell.x, cell.y))] = 1;
                        result.candidateCutMask.set(cell.x, cell.y, true);
                    }
                }
            }

            evidence.candidateCutCells =
                static_cast<int>(candidateCuts.size());
            evidence.removedRatio = evidence.componentCells > 0
                ? static_cast<double>(evidence.candidateCutCells) /
                    static_cast<double>(evidence.componentCells)
                : 0.0;

            if (!largeEnough) {
                evidence.decision = "component_too_small";
            } else if (evidence.lowRatio < options.minLowRatio) {
                evidence.decision = "insufficient_low_value_background";
            } else if (evidence.candidateCutCells < options.minRemovedCells) {
                evidence.decision = "insufficient_unsupported_cells";
            } else if (evidence.removedRatio > options.maxRemovedRatio) {
                evidence.decision = "removal_ratio_too_large";
            } else if (evidence.observedSupportComponents != 2) {
                evidence.decision = evidence.observedSupportComponents > 2
                    ? "ambiguous_support_component_count"
                    : "insufficient_support_components";
            } else {
                evidence.minimumResultingPartPhysicalAspect =
                    std::numeric_limits<double>::infinity();
                for (const Cell& start : cells) {
                    const std::size_t index = static_cast<std::size_t>(
                        indexOf(start.x, start.y));
                    if (splitVisited[index] || cutCell[index]) {
                        continue;
                    }
                    ++evidence.resultingComponents;
                    int partCells = 0;
                    double sumX = 0.0;
                    double sumY = 0.0;
                    double sumXX = 0.0;
                    double sumXY = 0.0;
                    double sumYY = 0.0;
                    std::queue<Cell> splitQueue;
                    splitQueue.push(start);
                    splitVisited[index] = 1;
                    while (!splitQueue.empty()) {
                        const Cell cell = splitQueue.front();
                        splitQueue.pop();
                        ++partCells;
                        const double worldX = metric.worldX(cell.x);
                        const double worldY = metric.worldY(cell.y);
                        sumX += worldX;
                        sumY += worldY;
                        sumXX += worldX * worldX;
                        sumXY += worldX * worldY;
                        sumYY += worldY * worldY;
                        for (const auto& offset : kOffsets) {
                            const int x = cell.x + offset[0];
                            const int y = cell.y + offset[1];
                            if (!grid.inBounds(x, y)) {
                                continue;
                            }
                            const std::size_t next = static_cast<std::size_t>(
                                indexOf(x, y));
                            if (!inComponent[next] || splitVisited[next] ||
                                cutCell[next]) {
                                continue;
                            }
                            splitVisited[next] = 1;
                            splitQueue.push({x, y});
                        }
                    }
                    if (partCells >= options.minPartCells &&
                        static_cast<double>(partCells) >=
                            minimumMajorPartCells) {
                        ++evidence.resultingMajorComponents;
                        evidence.minimumResultingPartPhysicalAspect = std::min(
                            evidence.minimumResultingPartPhysicalAspect,
                            physicalAspect(
                                partCells,
                                sumX,
                                sumY,
                                sumXX,
                                sumXY,
                                sumYY,
                                metric.minimumSpacing()));
                    }
                }
                if (evidence.resultingComponents != 2) {
                    evidence.decision = evidence.resultingComponents > 2
                        ? "ambiguous_multi_part_split"
                        : "does_not_split_component";
                } else if (evidence.resultingMajorComponents != 2) {
                    evidence.decision = "resulting_part_too_small";
                } else if (evidence.minimumResultingPartPhysicalAspect <
                    options.minResultingPartPhysicalAspect) {
                    evidence.decision = "insufficient_part_linearity";
                } else {
                    evidence.accepted = true;
                    evidence.decision = "accepted_support_envelope_split";
                }
            }

            result.stats.candidateCutCells += evidence.candidateCutCells;
            if (evidence.candidateCutCells > 0) {
                if (evidence.accepted) {
                    ++result.stats.acceptedComponents;
                    result.stats.acceptedCutCells +=
                        evidence.candidateCutCells;
                    for (const Cell& cell : candidateCuts) {
                        result.committedCutMask.set(cell.x, cell.y, true);
                    }
                } else {
                    ++result.stats.rejectedComponents;
                    result.stats.rejectedCutCells +=
                        evidence.candidateCutCells;
                }
            }
            if (result.decisions.size() < static_cast<std::size_t>(
                    options.maxEvidenceRecords)) {
                result.decisions.push_back(std::move(evidence));
            }

            for (const Cell& cell : cells) {
                const std::size_t index = static_cast<std::size_t>(
                    indexOf(cell.x, cell.y));
                inComponent[index] = 0;
                supportVisited[index] = 0;
                splitVisited[index] = 0;
                cutCell[index] = 0;
            }
        }
    }
    return result;
}

} // namespace fpt
