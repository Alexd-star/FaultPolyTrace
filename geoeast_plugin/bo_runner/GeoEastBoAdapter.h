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

// GeoEast's BOFaultPolygon schema stores closed line segments, but it has no
// field that identifies a segment as an interior ring.  FaultPolyTrace keeps
// holes in CSV/WKT and deliberately writes only exterior rings to GeoEast.
enum class FaultPolygonInteriorRingPolicy {
    OmitUnsupportedInteriorRings
};

struct FaultPolygonStorageSummary {
    int headerSegmentCount = 0;
    std::size_t segmentCount = 0;
    std::size_t closedSegmentCount = 0;
    std::size_t openSegmentCount = 0;
    std::size_t invalidSegmentCount = 0;
    std::size_t pointCount = 0;
    std::size_t uniqueSegmentIdCount = 0;
    std::size_t uniqueCombinedIdCount = 0;
    std::size_t uniquePrimaryKeyCount = 0;
    std::size_t duplicateSegmentIdCount = 0;
    std::size_t duplicatePrimaryKeyCount = 0;
    FaultPolygonStyle style;
};

std::vector<std::string> listMapGrids(const GeoEastProjectPtr& project);

std::vector<std::string> listFaultPolygons(const GeoEastProjectPtr& project);

bool faultPolygonsExist(
    const GeoEastProjectPtr& project,
    const std::string& outputName);

fpt::Grid2D readMapGrid(const GeoEastProjectPtr& project, const std::string& gridName);

std::vector<fpt::Polygon> readFaultPolygons(const GeoEastProjectPtr& project, const std::string& outputName);

FaultPolygonStyle readFaultPolygonStyle(
    const GeoEastProjectPtr& project,
    const std::string& outputName);

FaultPolygonStorageSummary inspectFaultPolygonStorage(
    const GeoEastProjectPtr& project,
    const std::string& outputName);

const char* faultPolygonInteriorRingPolicyName(
    FaultPolygonInteriorRingPolicy policy);

void validateMapGrid(const GeoEastProjectPtr& project, const std::string& gridName);

void writeMapGrid(
    const GeoEastProjectPtr& project,
    const std::string& projectName,
    const std::string& gridName,
    const fpt::Grid2D& grid,
    bool overwrite);

void deleteMapGrid(
    const GeoEastProjectPtr& project,
    const std::string& gridName);

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
