#include "WeakExtensionGate.h"

#include "GridMetric.h"

#include <algorithm>
#include <cmath>
#include <queue>
#include <set>
#include <stdexcept>

namespace fpt {
namespace {

double clamp01(double value) {
    return std::isfinite(value) ? std::max(0.0, std::min(1.0, value)) : 0.0;
}

} // namespace

void validateWeakExtensionGateOptions(const WeakExtensionGateOptions& options) {
    const bool finite = std::isfinite(options.smallMinAspect) &&
        std::isfinite(options.largeMinAspect) &&
        std::isfinite(options.smallMinSupportRatio) &&
        std::isfinite(options.largeMinSupportRatio) &&
        std::isfinite(options.smallMinMeanSupport) &&
        std::isfinite(options.largeMinMeanSupport) &&
        std::isfinite(options.maxEstimatedWidthSamples);
    if (!finite || options.smallComponentMaxCells < 1 ||
        options.maxComponentCells < 1 ||
        options.smallComponentMaxCells > options.maxComponentCells ||
        options.ownerSearchRadius < 1 ||
        options.smallMinAspect < 1.0 || options.largeMinAspect < 1.0 ||
        options.smallMinSupportRatio < 0.0 || options.smallMinSupportRatio > 1.0 ||
        options.largeMinSupportRatio < 0.0 || options.largeMinSupportRatio > 1.0 ||
        options.smallMinMeanSupport < 0.0 || options.smallMinMeanSupport > 1.0 ||
        options.largeMinMeanSupport < 0.0 || options.largeMinMeanSupport > 1.0 ||
        options.maxEstimatedWidthSamples <= 0.0) {
        throw std::runtime_error("Layered weak-extension gate options are out of range.");
    }
}

WeakExtensionOwnerMap buildWeakExtensionOwnerMap(const BinaryMask& strongMask) {
    WeakExtensionOwnerMap result;
    const int width = strongMask.width();
    const int height = strongMask.height();
    result.labels.assign(static_cast<std::size_t>(width * height), 0);
    std::queue<SkeletonPoint> queue;
    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const int start = y0 * width + x0;
            if (!strongMask.get(x0, y0) ||
                result.labels[static_cast<std::size_t>(start)] != 0) {
                continue;
            }
            ++result.componentCount;
            result.labels[static_cast<std::size_t>(start)] = result.componentCount;
            queue.push({x0, y0});
            while (!queue.empty()) {
                const SkeletonPoint point = queue.front();
                queue.pop();
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const int x = point.x + ox;
                        const int y = point.y + oy;
                        if (!strongMask.inBounds(x, y) || !strongMask.get(x, y)) {
                            continue;
                        }
                        const int position = y * width + x;
                        if (result.labels[static_cast<std::size_t>(position)] != 0) {
                            continue;
                        }
                        result.labels[static_cast<std::size_t>(position)] =
                            result.componentCount;
                        queue.push({x, y});
                    }
                }
            }
        }
    }
    return result;
}

WeakExtensionGateEvidence evaluateWeakExtensionComponent(
    const Grid2D& grid,
    const std::vector<SkeletonPoint>& cells,
    const WeakExtensionOwnerMap& owners,
    double weakThreshold,
    double strongThreshold,
    const WeakExtensionGateOptions& options) {
    validateWeakExtensionGateOptions(options);
    if (!std::isfinite(weakThreshold) || !std::isfinite(strongThreshold) ||
        strongThreshold <= weakThreshold) {
        throw std::runtime_error(
            "Layered weak-extension thresholds must be finite and strictly ordered.");
    }
    if (owners.labels.size() != grid.size()) {
        throw std::runtime_error("Weak-extension owner map must match the grid.");
    }

    WeakExtensionGateEvidence evidence;
    evidence.cellCount = static_cast<int>(cells.size());
    evidence.smallComponent = evidence.cellCount <= options.smallComponentMaxCells;
    if (cells.empty()) {
        evidence.decision = "empty_component";
        return evidence;
    }

    const GridMetric metric(grid);
    std::set<int> touchedOwners;
    double meanX = 0.0;
    double meanY = 0.0;
    double supportSum = 0.0;
    int supported = 0;
    const double supportThreshold = weakThreshold + 0.25 *
        (strongThreshold - weakThreshold);
    for (const SkeletonPoint& cell : cells) {
        if (!grid.inBounds(cell.x, cell.y)) {
            ++evidence.invalidValueCount;
            continue;
        }
        meanX += metric.worldDeltaX(static_cast<double>(cell.x));
        meanY += metric.worldDeltaY(static_cast<double>(cell.y));
        const double value = grid.value(cell.x, cell.y);
        if (std::isfinite(value)) {
            ++evidence.finiteValueCount;
            supportSum += clamp01((value - weakThreshold) /
                                  (strongThreshold - weakThreshold));
            supported += value >= supportThreshold ? 1 : 0;
        } else {
            ++evidence.invalidValueCount;
        }
        for (int oy = -options.ownerSearchRadius;
             oy <= options.ownerSearchRadius; ++oy) {
            for (int ox = -options.ownerSearchRadius;
                 ox <= options.ownerSearchRadius; ++ox) {
                const int x = cell.x + ox;
                const int y = cell.y + oy;
                if (!grid.inBounds(x, y)) {
                    continue;
                }
                const int owner = owners.labels[static_cast<std::size_t>(
                    y * grid.nx() + x)];
                if (owner > 0) {
                    touchedOwners.insert(owner);
                }
            }
        }
    }
    evidence.ownerCount = static_cast<int>(touchedOwners.size());
    const double finiteCount = static_cast<double>(
        std::max(1, evidence.finiteValueCount));
    evidence.supportRatio = static_cast<double>(supported) / finiteCount;
    evidence.meanSupport = supportSum / finiteCount;
    meanX /= static_cast<double>(cells.size());
    meanY /= static_cast<double>(cells.size());

    double xx = 0.0;
    double xy = 0.0;
    double yy = 0.0;
    double maxRadius = 0.0;
    for (const SkeletonPoint& cell : cells) {
        const double x = metric.worldDeltaX(static_cast<double>(cell.x)) - meanX;
        const double y = metric.worldDeltaY(static_cast<double>(cell.y)) - meanY;
        xx += x * x;
        xy += x * y;
        yy += y * y;
        maxRadius = std::max(maxRadius, std::hypot(x, y));
    }
    const double covarianceDenominator = std::max(1.0,
        static_cast<double>(cells.size() - 1));
    xx /= covarianceDenominator;
    xy /= covarianceDenominator;
    yy /= covarianceDenominator;
    const double trace = xx + yy;
    const double determinant = xx * yy - xy * xy;
    const double discriminant = std::sqrt(std::max(
        0.0, trace * trace - 4.0 * determinant));
    const double major = std::max(0.0, 0.5 * (trace + discriminant));
    const double minor = std::max(0.0, 0.5 * (trace - discriminant));
    const double samplingVariance = metric.minimumSpacing() *
        metric.minimumSpacing() / 12.0;
    evidence.physicalAspect = std::sqrt(
        (major + samplingVariance) / (minor + samplingVariance));
    evidence.physicalSpan = 2.0 * maxRadius + metric.minimumSpacing();
    evidence.estimatedWidthWorld =
        static_cast<double>(cells.size()) * metric.dx() * metric.dy() /
        std::max(metric.minimumSpacing(), evidence.physicalSpan);

    const double minAspect = evidence.smallComponent
        ? options.smallMinAspect : options.largeMinAspect;
    const double minSupport = evidence.smallComponent
        ? options.smallMinSupportRatio : options.largeMinSupportRatio;
    const double minMean = evidence.smallComponent
        ? options.smallMinMeanSupport : options.largeMinMeanSupport;
    const double maxWidth = options.maxEstimatedWidthSamples * metric.maximumSpacing();
    if (evidence.invalidValueCount > 0 || evidence.finiteValueCount == 0) {
        evidence.decision = "invalid_attribute_samples";
    } else if (evidence.ownerCount != 1) {
        evidence.decision = "ambiguous_or_missing_seed_owner";
    } else if (evidence.cellCount > options.maxComponentCells) {
        evidence.decision = "component_too_large";
    } else if (evidence.physicalAspect < minAspect) {
        evidence.decision = "insufficient_physical_linearity";
    } else if (evidence.supportRatio < minSupport || evidence.meanSupport < minMean) {
        evidence.decision = "insufficient_attribute_support";
    } else if (!evidence.smallComponent && evidence.estimatedWidthWorld > maxWidth) {
        evidence.decision = "unstable_or_excessive_width";
    } else {
        evidence.accepted = true;
        evidence.decision = "accepted";
    }
    return evidence;
}

} // namespace fpt
