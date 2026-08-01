#include "Segmentation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fpt {
namespace {

PercentileThresholdInfo computeThresholdInfo(
    const Grid2D& grid,
    double percentile,
    bool positiveOnly,
    double positiveFloor) {
    if (percentile <= 0.0 || percentile >= 1.0) {
        throw std::runtime_error(positiveOnly
            ? "Positive percentile threshold must be between 0 and 1."
            : "Percentile threshold must be between 0 and 1.");
    }

    std::vector<double> values;
    values.reserve(grid.size());
    for (const double value : grid.values()) {
        if (std::isfinite(value) && (!positiveOnly || value > positiveFloor)) {
            values.push_back(value);
        }
    }
    if (values.empty()) {
        throw std::runtime_error(positiveOnly
            ? "Grid has no positive finite values for positive percentile thresholding."
            : "Grid has no finite values for percentile thresholding.");
    }

    const std::size_t index = static_cast<std::size_t>(std::floor(
        percentile * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(),
                     values.begin() + static_cast<std::ptrdiff_t>(index),
                     values.end());
    const double threshold = values[index];
    std::size_t lessCount = 0;
    std::size_t equalCount = 0;
    for (const double value : values) {
        lessCount += value < threshold ? 1U : 0U;
        equalCount += value == threshold ? 1U : 0U;
    }

    PercentileThresholdInfo info;
    info.threshold = threshold;
    info.sampleCount = values.size();
    info.equalValueCount = equalCount;
    info.percentileBased = true;
    if (values.size() > 1) {
        const double denominator = static_cast<double>(values.size() - 1);
        info.stablePercentileLowerInclusive =
            static_cast<double>(lessCount) / denominator;
        info.stablePercentileUpperExclusive = std::min(
            1.0,
            static_cast<double>(lessCount + equalCount) / denominator);
    }
    return info;
}

} // namespace

double computePercentileThreshold(const Grid2D& grid, double percentile) {
    return computePercentileThresholdInfo(grid, percentile).threshold;
}

double computePositivePercentileThreshold(const Grid2D& grid, double percentile, double positiveFloor) {
    return computePositivePercentileThresholdInfo(
        grid, percentile, positiveFloor).threshold;
}

PercentileThresholdInfo computePercentileThresholdInfo(
    const Grid2D& grid, double percentile) {
    return computeThresholdInfo(grid, percentile, false, 0.0);
}

PercentileThresholdInfo computePositivePercentileThresholdInfo(
    const Grid2D& grid, double percentile, double positiveFloor) {
    return computeThresholdInfo(grid, percentile, true, positiveFloor);
}

SegmentationResult segmentFaultAttributes(const Grid2D& grid, const SegmentationOptions& options) {
    if (grid.empty()) {
        throw std::runtime_error("Cannot segment an empty grid.");
    }
    if (options.openRadius < 0 || options.closeRadius < 0) {
        throw std::runtime_error("Morphology radii must be non-negative.");
    }

    SegmentationResult result{BinaryMask(grid.nx(), grid.ny()), 0.0, {}};
    if (options.thresholdMode == ThresholdMode::Percentile) {
        result.thresholdInfo = computePercentileThresholdInfo(
            grid, options.percentile);
        result.effectiveThreshold = result.thresholdInfo.threshold;
    } else if (options.thresholdMode == ThresholdMode::PositivePercentile) {
        result.thresholdInfo = computePositivePercentileThresholdInfo(
            grid, options.percentile);
        result.effectiveThreshold = result.thresholdInfo.threshold;
    } else {
        result.effectiveThreshold = options.threshold;
        result.thresholdInfo.threshold = options.threshold;
    }

    for (int y = 0; y < grid.ny(); ++y) {
        for (int x = 0; x < grid.nx(); ++x) {
            const double v = grid.value(x, y);
            result.mask.set(x, y, std::isfinite(v) && v >= result.effectiveThreshold);
        }
    }

    if (options.openRadius > 0) {
        result.mask = openMask(result.mask, options.openRadius);
    }
    if (options.closeRadius > 0) {
        result.mask = closeMask(result.mask, options.closeRadius);
    }

    return result;
}

} // namespace fpt
