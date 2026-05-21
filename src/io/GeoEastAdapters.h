#pragma once

// This file intentionally contains interface sketches only.
// The real implementation requires GeoEast/iEco headers and libraries.
//
// Planned mapping:
// - BOMapGrid -> fpt::Grid2D
// - fpt::Polygon vector -> BOFaultPolygon via BAFaultPolygonHead and BAFaultPolygonData

namespace fpt {

class GeoEastMapGridReader {
public:
    // Future method:
    // Grid2D read(const std::string& projectName, const std::string& gridName);
};

class GeoEastFaultPolygonWriter {
public:
    // Future method:
    // void write(const std::string& projectName, const std::string& outputName, const std::vector<Polygon>& polygons);
};

} // namespace fpt

