#include "Segmentation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace fpt {

double computePercentileThreshold(const Grid2D& grid, double percentile) {
    if (percentile <= 0.0 || percentile >= 1.0) {
        throw std::runtime_error("Percentile threshold must be between 0 and 1.");
    }

    std::vector<double> values;
    values.reserve(grid.size());
    for (const double value : grid.values()) {
        if (std::isfinite(value)) {
            values.push_back(value);
        }
    }
    if (values.empty()) {
        throw std::runtime_error("Grid has no finite values for percentile thresholding.");
    }

    const std::size_t index = static_cast<std::size_t>(std::floor(percentile * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

double computePositivePercentileThreshold(const Grid2D& grid, double percentile, double positiveFloor) {
    if (percentile <= 0.0 || percentile >= 1.0) {
        throw std::runtime_error("Positive percentile threshold must be between 0 and 1.");
    }

    std::vector<double> values;
    values.reserve(grid.size());
    for (const double value : grid.values()) {
        if (std::isfinite(value) && value > positiveFloor) {
            values.push_back(value);
        }
    }
    if (values.empty()) {
        throw std::runtime_error("Grid has no positive finite values for positive percentile thresholding.");
    }

    const std::size_t index = static_cast<std::size_t>(std::floor(percentile * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

SegmentationResult segmentFaultAttributes(const Grid2D& grid, const SegmentationOptions& options) {
    if (grid.empty()) {
        throw std::runtime_error("Cannot segment an empty grid.");
    }
    if (options.openRadius < 0 || options.closeRadius < 0) {
        throw std::runtime_error("Morphology radii must be non-negative.");
    }

    SegmentationResult result{BinaryMask(grid.nx(), grid.ny()), 0.0};
    if (options.thresholdMode == ThresholdMode::Percentile) {
        result.effectiveThreshold = computePercentileThreshold(grid, options.percentile);
    } else if (options.thresholdMode == ThresholdMode::PositivePercentile) {
        result.effectiveThreshold = computePositivePercentileThreshold(grid, options.percentile);
    } else {
        result.effectiveThreshold = options.threshold;
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
