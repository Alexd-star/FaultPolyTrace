#pragma once

#include "Grid2D.h"

#include <vector>

namespace fpt {

struct GridOffset {
    int x = 0;
    int y = 0;
    double distance = 0.0;
};

class GridMetric {
public:
    explicit GridMetric(const GridMetadata& metadata);
    explicit GridMetric(const Grid2D& grid) : GridMetric(grid.metadata()) {}

    int nx() const { return metadata_.nx; }
    int ny() const { return metadata_.ny; }
    double dx() const { return metadata_.dx; }
    double dy() const { return metadata_.dy; }

    double worldDeltaX(double gridDeltaX) const;
    double worldDeltaY(double gridDeltaY) const;
    double worldX(double gridX) const;
    double worldY(double gridY) const;
    double distance(double x0, double y0, double x1, double y1) const;
    double directionDegrees(double x0, double y0, double x1, double y1) const;
    double minimumSpacing() const;
    double maximumSpacing() const;
    bool isIsotropic(double relativeTolerance = 1e-9) const;

    std::vector<GridOffset> offsetsWithinRadius(
        double radiusWorld,
        bool includeCenter = true,
        std::size_t maximumOffsets = 1000000) const;

private:
    GridMetadata metadata_;
};

} // namespace fpt
