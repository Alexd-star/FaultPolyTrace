#pragma once

#include "../../src/core/Grid2D.h"
#include "../../src/core/Polygon.h"

#include <memory>
#include <string>
#include <vector>

#include "boproject.h"

namespace fpt_geoeast {

using GeoEastProjectPtr = std::shared_ptr<ngp::bo::BOProject>;

struct FaultPolygonStyle {
    int red = 0;
    int green = 255;
    int blue = 255;
    double lineWidth = 0.025;
};

std::vector<std::string> listMapGrids(const GeoEastProjectPtr& project);

fpt::Grid2D readMapGrid(const GeoEastProjectPtr& project, const std::string& gridName);

void validateMapGrid(const GeoEastProjectPtr& project, const std::string& gridName);

void writeMapGrid(
    const GeoEastProjectPtr& project,
    const std::string& projectName,
    const std::string& gridName,
    const fpt::Grid2D& grid,
    bool overwrite);

void writeFaultPolygons(
    const GeoEastProjectPtr& project,
    const std::string& outputName,
    const std::vector<fpt::Polygon>& polygons,
    bool keptOnly,
    const FaultPolygonStyle& style = FaultPolygonStyle{});

void deleteFaultPolygons(
    const GeoEastProjectPtr& project,
    const std::string& outputName);

} // namespace fpt_geoeast
