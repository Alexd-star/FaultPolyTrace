#include "Grid2D.h"

#include <cmath>
#include <limits>
#include <stdexcept>

namespace fpt {

Grid2D::Grid2D(GridMetadata metadata) : metadata_(metadata) {
    validate();
    const std::size_t nx = static_cast<std::size_t>(metadata_.nx);
    const std::size_t ny = static_cast<std::size_t>(metadata_.ny);
    if (nx > std::numeric_limits<std::size_t>::max() / ny ||
        nx * ny > values_.max_size()) {
        throw std::runtime_error("Grid dimensions exceed the supported memory range.");
    }
    values_.assign(nx * ny, 0.0);
}

double Grid2D::value(int x, int y) const {
    return values_.at(static_cast<std::size_t>(y) * metadata_.nx + x);
}

double& Grid2D::value(int x, int y) {
    return values_.at(static_cast<std::size_t>(y) * metadata_.nx + x);
}

bool Grid2D::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < metadata_.nx && y < metadata_.ny;
}

double Grid2D::worldX(double gridX) const {
    return metadata_.sx + gridX * metadata_.dx;
}

double Grid2D::worldY(double gridY) const {
    return metadata_.sy + gridY * metadata_.dy;
}

void Grid2D::validate() const {
    if (metadata_.nx <= 0 || metadata_.ny <= 0) {
        throw std::runtime_error("Grid dimensions must be positive.");
    }
    if (!std::isfinite(metadata_.sx) || !std::isfinite(metadata_.sy)) {
        throw std::runtime_error("Grid origin must contain finite coordinates.");
    }
    if (!std::isfinite(metadata_.dx) || !std::isfinite(metadata_.dy) ||
        metadata_.dx <= 0.0 || metadata_.dy <= 0.0) {
        throw std::runtime_error("Grid spacing must be finite and positive.");
    }
}

} // namespace fpt
