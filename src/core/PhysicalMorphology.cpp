#include "PhysicalMorphology.h"

#include <stdexcept>
#include <vector>

namespace fpt {
namespace {

void validateDimensions(const BinaryMask& mask, const GridMetric& metric) {
    if (mask.width() != metric.nx() || mask.height() != metric.ny()) {
        throw std::runtime_error("Physical morphology mask and grid dimensions must match.");
    }
}

} // namespace

BinaryMask dilatePhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets) {
    validateDimensions(mask, metric);
    const std::vector<GridOffset> offsets =
        metric.offsetsWithinRadius(radiusWorld, true, maximumOffsets);
    BinaryMask output(mask.width(), mask.height());
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            for (const GridOffset& offset : offsets) {
                output.set(x + offset.x, y + offset.y, true);
            }
        }
    }
    return output;
}

BinaryMask erodePhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets) {
    validateDimensions(mask, metric);
    const std::vector<GridOffset> offsets =
        metric.offsetsWithinRadius(radiusWorld, true, maximumOffsets);
    BinaryMask output(mask.width(), mask.height());
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            if (!mask.get(x, y)) {
                continue;
            }
            bool allActive = true;
            for (const GridOffset& offset : offsets) {
                if (!mask.get(x + offset.x, y + offset.y)) {
                    allActive = false;
                    break;
                }
            }
            output.set(x, y, allActive);
        }
    }
    return output;
}

BinaryMask openMaskPhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets) {
    return dilatePhysical(
        erodePhysical(mask, metric, radiusWorld, maximumOffsets),
        metric, radiusWorld, maximumOffsets);
}

BinaryMask closeMaskPhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets) {
    return erodePhysical(
        dilatePhysical(mask, metric, radiusWorld, maximumOffsets),
        metric, radiusWorld, maximumOffsets);
}

} // namespace fpt
