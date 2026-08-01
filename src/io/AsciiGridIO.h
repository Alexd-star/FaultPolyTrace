#pragma once

#include "../core/Grid2D.h"
#include "../core/Polygon.h"
#include "../core/TraceResult.h"

#include <string>
#include <vector>

namespace fpt {

Grid2D readAsciiGrid(const std::string& path);
Grid2D readGeoEastDatGrid(const std::string& path);
Grid2D readAutoGrid(const std::string& path);
std::vector<Polygon> readKeptPolygonCsv(const std::string& path);
void writePolygonCsv(const std::string& path, const std::vector<Polygon>& polygons);
void writeBranchCsv(const std::string& path, const std::vector<BranchTrace>& branches);

} // namespace fpt
