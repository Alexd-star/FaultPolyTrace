#include "../src/core/Segmentation.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    fpt::GridMetadata metadata;
    metadata.nx = 5;
    metadata.ny = 1;
    fpt::Grid2D grid(metadata);
    grid.values() = {0.0, 0.0, 0.0, 1.0, 1.0};

    const fpt::PercentileThresholdInfo lower =
        fpt::computePercentileThresholdInfo(grid, 0.50);
    require(lower.threshold == 0.0,
            "The median must remain on the zero-value plateau.");
    require(std::abs(lower.stablePercentileLowerInclusive - 0.0) < 1e-12 &&
                std::abs(lower.stablePercentileUpperExclusive - 0.75) < 1e-12,
            "The zero threshold must report its complete percentile plateau.");
    require(lower.sampleCount == 5 && lower.equalValueCount == 3,
            "Threshold diagnostics must report sample and tie counts.");

    const fpt::PercentileThresholdInfo upper =
        fpt::computePercentileThresholdInfo(grid, 0.76);
    require(upper.threshold == 1.0,
            "A percentile beyond the plateau boundary must select the next value.");
    require(std::abs(upper.stablePercentileLowerInclusive - 0.75) < 1e-12 &&
                std::abs(upper.stablePercentileUpperExclusive - 1.0) < 1e-12,
            "The upper threshold plateau must end at one.");

    std::cout << "Segmentation threshold-info tests passed.\n";
    return 0;
}
