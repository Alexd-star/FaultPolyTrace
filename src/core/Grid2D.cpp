#include "Grid2D.h"

#include <stdexcept>

namespace fpt {

Grid2D::Grid2D(GridMetadata metadata) : metadata_(metadata) {
    validate();
    values_.assign(static_cast<std::size_t>(metadata_.nx) * static_cast<std::size_t>(metadata_.ny), 0.0);
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
    if (metadata_.dx <= 0.0 || metadata_.dy <= 0.0) {
        throw std::runtime_error("Grid spacing must be positive.");
    }
}

} // namespace fpt

