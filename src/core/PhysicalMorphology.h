#pragma once

#include "BinaryMask.h"
#include "GridMetric.h"

namespace fpt {

BinaryMask dilatePhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets = 1000000);
BinaryMask erodePhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets = 1000000);
BinaryMask openMaskPhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets = 1000000);
BinaryMask closeMaskPhysical(
    const BinaryMask& mask,
    const GridMetric& metric,
    double radiusWorld,
    std::size_t maximumOffsets = 1000000);

} // namespace fpt
