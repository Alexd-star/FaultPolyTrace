#include "GridMetric.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace fpt {

GridMetric::GridMetric(const GridMetadata& metadata) : metadata_(metadata) {
    if (metadata_.nx <= 0 || metadata_.ny <= 0) {
        throw std::runtime_error("GridMetric dimensions must be positive.");
    }
    if (!std::isfinite(metadata_.dx) || !std::isfinite(metadata_.dy) ||
        metadata_.dx <= 0.0 || metadata_.dy <= 0.0) {
        throw std::runtime_error("GridMetric spacing must be finite and positive.");
    }
}

double GridMetric::worldDeltaX(double gridDeltaX) const {
    return gridDeltaX * metadata_.dx;
}

double GridMetric::worldDeltaY(double gridDeltaY) const {
    return gridDeltaY * metadata_.dy;
}

double GridMetric::worldX(double gridX) const {
    return metadata_.sx + gridX * metadata_.dx;
}

double GridMetric::worldY(double gridY) const {
    return metadata_.sy + gridY * metadata_.dy;
}

double GridMetric::distance(double x0, double y0, double x1, double y1) const {
    return std::hypot(worldDeltaX(x1 - x0), worldDeltaY(y1 - y0));
}

double GridMetric::directionDegrees(double x0, double y0, double x1, double y1) const {
    constexpr double radiansToDegrees = 57.2957795130823208768;
    return std::atan2(worldDeltaY(y1 - y0), worldDeltaX(x1 - x0)) *
        radiansToDegrees;
}

double GridMetric::minimumSpacing() const {
    return std::min(metadata_.dx, metadata_.dy);
}

double GridMetric::maximumSpacing() const {
    return std::max(metadata_.dx, metadata_.dy);
}

bool GridMetric::isIsotropic(double relativeTolerance) const {
    if (!std::isfinite(relativeTolerance) || relativeTolerance < 0.0) {
        throw std::runtime_error("GridMetric isotropy tolerance must be finite and non-negative.");
    }
    return std::abs(metadata_.dx - metadata_.dy) <=
        relativeTolerance * maximumSpacing();
}

std::vector<GridOffset> GridMetric::offsetsWithinRadius(
    double radiusWorld,
    bool includeCenter,
    std::size_t maximumOffsets) const {
    if (!std::isfinite(radiusWorld) || radiusWorld < 0.0) {
        throw std::runtime_error("Physical morphology radius must be finite and non-negative.");
    }
    if (maximumOffsets == 0) {
        throw std::runtime_error("Physical morphology offset budget must be positive.");
    }

    const double maxXDouble = std::floor(radiusWorld / metadata_.dx + 1e-12);
    const double maxYDouble = std::floor(radiusWorld / metadata_.dy + 1e-12);
    if (maxXDouble > static_cast<double>(std::numeric_limits<int>::max()) ||
        maxYDouble > static_cast<double>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("Physical morphology radius exceeds the supported index range.");
    }
    const int maxX = static_cast<int>(maxXDouble);
    const int maxY = static_cast<int>(maxYDouble);
    const std::size_t width = static_cast<std::size_t>(maxX) * 2U + 1U;
    const std::size_t height = static_cast<std::size_t>(maxY) * 2U + 1U;
    if (height != 0 && width > maximumOffsets / height) {
        throw std::runtime_error("Physical morphology radius exceeds the configured offset budget.");
    }

    std::vector<GridOffset> offsets;
    offsets.reserve(std::min(maximumOffsets, width * height));
    const double radiusSquared = radiusWorld * radiusWorld;
    const double tolerance = std::max(1.0, radiusSquared) * 1e-12;
    for (int y = -maxY; y <= maxY; ++y) {
        for (int x = -maxX; x <= maxX; ++x) {
            if (!includeCenter && x == 0 && y == 0) {
                continue;
            }
            const double physicalX = worldDeltaX(static_cast<double>(x));
            const double physicalY = worldDeltaY(static_cast<double>(y));
            const double distanceSquared = physicalX * physicalX + physicalY * physicalY;
            if (distanceSquared <= radiusSquared + tolerance) {
                offsets.push_back({x, y, std::sqrt(distanceSquared)});
            }
        }
    }
    if (includeCenter && offsets.empty()) {
        offsets.push_back({0, 0, 0.0});
    }
    return offsets;
}

} // namespace fpt
