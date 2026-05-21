#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"

namespace fpt {

enum class ThresholdMode {
    Fixed,
    Percentile,
    PositivePercentile
};

struct SegmentationOptions {
    ThresholdMode thresholdMode = ThresholdMode::Fixed;
    double threshold = 0.5;
    double percentile = 0.90;
    int openRadius = 0;
    int closeRadius = 0;
};

struct SegmentationResult {
    BinaryMask mask;
    double effectiveThreshold = 0.0;
};

double computePercentileThreshold(const Grid2D& grid, double percentile);
double computePositivePercentileThreshold(const Grid2D& grid, double percentile, double positiveFloor = 0.0);
SegmentationResult segmentFaultAttributes(const Grid2D& grid, const SegmentationOptions& options);

} // namespace fpt
