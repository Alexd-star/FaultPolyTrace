#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "Polygon.h"

#include <cstddef>
#include <vector>

namespace fpt {

struct PolygonRasterQuality {
    int insideCells = 0;
    int supportedCells = 0;
    int backgroundCells = 0;
    std::size_t evaluatedCells = 0;
    bool budgetExceeded = false;
};

struct PolygonDualSupportQuality {
    int insideCells = 0;
    int strongSupportedCells = 0;
    int weakSupportedCells = 0;
    int backgroundCells = 0;
    std::size_t evaluatedCells = 0;
    bool budgetExceeded = false;
};

struct PolygonCellCoverage {
    int firstX = 0;
    int firstY = 0;
    int width = 0;
    int height = 0;
    std::size_t evaluatedCells = 0;
    bool budgetExceeded = false;
    std::vector<unsigned char> covered;

    bool get(int x, int y) const {
        if (x < firstX || y < firstY ||
            x >= firstX + width || y >= firstY + height) {
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(y - firstY) *
            static_cast<std::size_t>(width) +
            static_cast<std::size_t>(x - firstX);
        return index < covered.size() && covered[index] != 0;
    }
};

bool pointInPolygonArea(
    const Point& point,
    const Polygon& polygon);

bool cellIntersectsPolygonArea(
    const Grid2D& grid,
    int x,
    int y,
    const Polygon& polygon);

PolygonCellCoverage rasterizePolygonCellCoverage(
    const Grid2D& grid,
    const Polygon& polygon,
    std::size_t maximumEvaluatedCells);

PolygonRasterQuality measurePolygonRasterQuality(
    const Grid2D& grid,
    const BinaryMask& supportMask,
    const Polygon& polygon,
    double supportThreshold,
    std::size_t maximumEvaluatedCells);

PolygonDualSupportQuality measurePolygonDualSupportQuality(
    const Grid2D& grid,
    const BinaryMask& strongSupportMask,
    const BinaryMask& weakSupportMask,
    const Polygon& polygon,
    double strongSupportThreshold,
    double weakSupportThreshold,
    std::size_t maximumEvaluatedCells,
    double backgroundThreshold = 0.05);

double polygonBoundaryRoughness(const Polygon& polygon);

void refreshPolygonGeometry(Polygon& polygon);

} // namespace fpt
