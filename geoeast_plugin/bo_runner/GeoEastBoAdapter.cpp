#include "GeoEastBoAdapter.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <direct.h>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "bomapgrid.h"
#include "bofaultpolygon.h"
#include "brfault.h"

namespace fpt_geoeast {
using namespace ngp::dp;
namespace geobo = ngp::bo;
namespace {

bool isValidGridValue(double value) {
    return std::isfinite(value) &&
           std::abs(value) <= 0.1 * static_cast<double>(FLT_MAX);
}

double stableStoredGridValue(double value, bool normalizedAttributeGrid) {
    if (!isValidGridValue(value)) {
        return 0.0;
    }
    if (normalizedAttributeGrid) {
        return std::round(value * 1.0e6) / 1.0e6;
    }
    return value;
}

void updateBounds(const fpt::Point& point, double& minX, double& minY, double& maxX, double& maxY) {
    minX = std::min(minX, point.x);
    minY = std::min(minY, point.y);
    maxX = std::max(maxX, point.x);
    maxY = std::max(maxY, point.y);
}

} // namespace

std::vector<std::string> listMapGrids(const GeoEastProjectPtr& project) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }

    geobo::BANameIDs nameIds;
    project->listNameIDsOfChildren<geobo::BOMapGrid>(nameIds);

    std::vector<std::string> names;
    names.reserve(nameIds.size());
    for (int i = 0; i < static_cast<int>(nameIds.size()); ++i) {
        names.push_back(nameIds[i]->getName());
    }
    return names;
}

std::vector<std::string> listFaultPolygons(const GeoEastProjectPtr& project) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }

    geobo::BANameIDs nameIds;
    project->listNameIDsOfChildren<geobo::BOFaultPolygon>(nameIds);

    std::vector<std::string> names;
    names.reserve(nameIds.size());
    for (int i = 0; i < static_cast<int>(nameIds.size()); ++i) {
        names.push_back(nameIds[i]->getName());
    }
    return names;
}

bool faultPolygonsExist(
    const GeoEastProjectPtr& project,
    const std::string& outputName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (outputName.empty()) {
        return false;
    }
    return static_cast<bool>(
        project->getChild<geobo::BOFaultPolygon>(outputName));
}

std::vector<fpt::Polygon> readFaultPolygons(const GeoEastProjectPtr& project, const std::string& outputName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (outputName.empty()) {
        throw std::runtime_error("GeoEast BOFaultPolygon output name is empty.");
    }

    std::shared_ptr<geobo::BOFaultPolygon> boPolygon = project->getChild<geobo::BOFaultPolygon>(outputName);
    if (!boPolygon) {
        throw std::runtime_error("Failed to get BOFaultPolygon: " + outputName);
    }

    BAFaultPolygonHead head;
    BAFaultPolygonData data;
    if (!boPolygon->readData(head, data)) {
        throw std::runtime_error("Failed to read BOFaultPolygon data: " + outputName);
    }

    std::vector<fpt::Polygon> polygons;
    polygons.reserve(data.faultPolygonList.size());
    int id = 1;
    for (const BAFaultPolygonSegment& segment : data.faultPolygonList) {
        if (segment.closeFlag == 0) {
            throw std::runtime_error(
                "BOFaultPolygon contains an open segment that cannot be represented "
                "as a closed FaultPolyTrace polygon: " +
                std::to_string(static_cast<long long>(segment.faultPolygonSegID)));
        }
        if (segment.faultPolygonData.size() < 3) {
            continue;
        }
        fpt::Polygon polygon;
        polygon.id = id++;
        polygon.kept = true;
        polygon.points.reserve(segment.faultPolygonData.size());
        for (const BAFaultPolygonPoint& sourcePoint : segment.faultPolygonData) {
            polygon.points.push_back({sourcePoint.x, sourcePoint.y});
        }
        if (polygon.points.size() >= 2) {
            const fpt::Point& first = polygon.points.front();
            const fpt::Point& last = polygon.points.back();
            if (std::abs(first.x - last.x) <= 1e-9 && std::abs(first.y - last.y) <= 1e-9) {
                polygon.points.pop_back();
            }
        }
        if (polygon.points.size() < 3) {
            continue;
        }
        polygon.area = fpt::polygonArea(polygon.points);
        polygon.perimeter = fpt::polygonPerimeter(polygon.points);
        polygon.cellCount = static_cast<int>(polygon.points.size());
        polygons.push_back(std::move(polygon));
    }
    return polygons;
}

FaultPolygonStyle readFaultPolygonStyle(
    const GeoEastProjectPtr& project,
    const std::string& outputName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (outputName.empty()) {
        throw std::runtime_error("GeoEast BOFaultPolygon output name is empty.");
    }
    std::shared_ptr<geobo::BOFaultPolygon> boPolygon =
        project->getChild<geobo::BOFaultPolygon>(outputName);
    if (!boPolygon) {
        throw std::runtime_error("Failed to get BOFaultPolygon: " + outputName);
    }
    BAFaultPolygonHead head;
    BAFaultPolygonData data;
    if (!boPolygon->readData(head, data)) {
        throw std::runtime_error("Failed to read BOFaultPolygon style: " + outputName);
    }
    FaultPolygonStyle style;
    style.red = head.red;
    style.green = head.green;
    style.blue = head.blue;
    style.lineWidth = head.linewidth;
    return style;
}

fpt::Grid2D readMapGrid(const GeoEastProjectPtr& project, const std::string& gridName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }

    std::shared_ptr<geobo::BOMapGrid> boGrid = project->getChild<geobo::BOMapGrid>(gridName);
    if (!boGrid) {
        throw std::runtime_error("Failed to get BOMapGrid: " + gridName);
    }

    boGrid->readDataHead();
    boGrid->readData();
    BAMapGridHead head = boGrid->getDataHead();

    fpt::GridMetadata metadata;
    metadata.nx = head.nx;
    metadata.ny = head.ny;
    // BOMapGrid stores the first sample center, while Grid2D stores the
    // lower-left cell boundary used by polygon rings.
    metadata.sx = head.sx - 0.5 * std::abs(head.dx);
    metadata.sy = head.sy - 0.5 * std::abs(head.dy);
    metadata.dx = head.dx;
    metadata.dy = head.dy;
    fpt::Grid2D grid(metadata);
    double observedMin = std::numeric_limits<double>::infinity();
    double observedMax = -std::numeric_limits<double>::infinity();
    for (int i = 0; i < head.nx; ++i) {
        for (int j = 0; j < head.ny; ++j) {
            BAMapGridPoint point = boGrid->getData(i, j);
            const double value = isValidGridValue(point.z) ? point.z : 0.0;
            grid.value(i, j) = value;
            observedMin = std::min(observedMin, value);
            observedMax = std::max(observedMax, value);
        }
    }
    const bool normalizedAttributeGrid =
        observedMin >= -1.000001 && observedMax <= 1.000001;
    if (normalizedAttributeGrid) {
        for (int i = 0; i < head.nx; ++i) {
            for (int j = 0; j < head.ny; ++j) {
                grid.value(i, j) = stableStoredGridValue(grid.value(i, j), true);
            }
        }
    }
    return grid;
}

void validateMapGrid(const GeoEastProjectPtr& project, const std::string& gridName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }

    std::shared_ptr<geobo::BOMapGrid> boGrid = project->getChild<geobo::BOMapGrid>(gridName);
    if (!boGrid) {
        throw std::runtime_error("Failed to get BOMapGrid: " + gridName);
    }

    boGrid->readDataHead();
    BAMapGridHead head = boGrid->getDataHead();
    std::cout << "BOMapGrid validated: " << gridName << "\n";
    std::cout << "Grid head: sx=" << head.sx
              << " sy=" << head.sy
              << " nx=" << head.nx
              << " ny=" << head.ny
              << " dx=" << head.dx
              << " dy=" << head.dy << "\n";
}

void writeMapGrid(
    const GeoEastProjectPtr& project,
    const std::string& projectName,
    const std::string& gridName,
    const fpt::Grid2D& grid,
    bool overwrite) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (gridName.empty()) {
        throw std::runtime_error("GeoEast BOMapGrid output name is empty.");
    }
    grid.validate();

    std::shared_ptr<geobo::BOMapGrid> existingGrid = project->getChild<geobo::BOMapGrid>(gridName);
    if (existingGrid) {
        if (!overwrite) {
            throw std::runtime_error("BOMapGrid already exists. Use --overwrite-grid to replace it: " + gridName);
        }
        project->eraseChild<geobo::BOMapGrid>(gridName);
    }

    const char* ngpData = std::getenv("NGPDATA");
    if (!ngpData || !*ngpData) {
        throw std::runtime_error("Missing GeoEast environment variable: NGPDATA");
    }

    const std::string projectPath = std::string(ngpData) + "/" + projectName;
    const std::string mappingsPath = projectPath + "/mappings";
    const std::string griddingsPath = mappingsPath + "/griddings";
    _mkdir(mappingsPath.c_str());
    _mkdir(griddingsPath.c_str());

    std::shared_ptr<geobo::BOMapGrid> boGrid = project->createChild<geobo::BOMapGrid>(gridName);
    if (!boGrid) {
        throw std::runtime_error("Failed to create BOMapGrid: " + gridName);
    }

    BAMapGridHead head;
    head.nx = grid.nx();
    head.ny = grid.ny();
    head.sx = grid.sx() + 0.5 * grid.dx();
    head.sy = grid.sy() + 0.5 * grid.dy();
    head.dx = grid.dx();
    head.dy = grid.dy();
    head.angle = 0.0;
    head.parentID = 0;
    head.faultPolygonID = 0;
    head.surveyName = "";
    head.minValue = std::numeric_limits<double>::max();
    head.maxValue = -std::numeric_limits<double>::max();

    std::vector<BAMapGridVector> data(static_cast<std::size_t>(grid.nx()));
    for (int i = 0; i < grid.nx(); ++i) {
        data[static_cast<std::size_t>(i)].resize(static_cast<std::size_t>(grid.ny()));
        for (int j = 0; j < grid.ny(); ++j) {
            const double value = grid.value(i, j);
            BAMapGridPoint point;
            point.x = grid.worldX(i + 0.5);
            point.y = grid.worldY(j + 0.5);
            point.z = value;
            data[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = point;
            if (std::isfinite(value)) {
                head.minValue = std::min(head.minValue, value);
                head.maxValue = std::max(head.maxValue, value);
            }
        }
    }

    if (head.minValue == std::numeric_limits<double>::max()) {
        head.minValue = 0.0;
        head.maxValue = 0.0;
    }

    boGrid->setDataHead(head);
    boGrid->setVersion(FMT_MAPGRID_BIN_V3);
    boGrid->setFileName(griddingsPath + "/" + gridName);

    if (!boGrid->saveData(head, data)) {
        throw std::runtime_error("Failed to write BOMapGrid data: " + gridName);
    }

    std::cout << "BOMapGrid written: " << gridName << "\n";
    std::cout << "BOMapGrid file: " << griddingsPath << "/" << gridName << "\n";
}

void deleteMapGrid(
    const GeoEastProjectPtr& project,
    const std::string& gridName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (gridName.empty()) {
        throw std::runtime_error("GeoEast BOMapGrid name is empty.");
    }
    if (!project->getChild<geobo::BOMapGrid>(gridName)) {
        throw std::runtime_error("BOMapGrid does not exist: " + gridName);
    }
    NStatus eraseResult = project->eraseChild<geobo::BOMapGrid>(gridName);
    if (eraseResult.Value() == NStatus::NFail) {
        throw std::runtime_error("Failed to delete BOMapGrid: " + gridName);
    }
    std::cout << "BOMapGrid deleted: " << gridName << "\n";
}

void writeFaultPolygons(
    const GeoEastProjectPtr& project,
    const std::string& outputName,
    const std::vector<fpt::Polygon>& polygons,
    bool keptOnly,
    const FaultPolygonStyle& style) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (outputName.empty()) {
        throw std::runtime_error("GeoEast BOFaultPolygon output name is empty.");
    }
    std::size_t omittedHoleCount = 0;
    for (const fpt::Polygon& polygon : polygons) {
        if (!keptOnly || polygon.kept) {
            omittedHoleCount += polygon.holes.size();
        }
    }
    if (omittedHoleCount > 0) {
        // BOFaultPolygon has no parent-ring field for interior rings.  Writing
        // them as independent segments would create false fault polygons, so
        // the GeoEast display layer keeps each exterior ring and fills holes.
        // The full polygon-with-holes geometry remains available in CSV/WKT.
        std::cerr << "GeoEast compatibility: omitted " << omittedHoleCount
                  << " interior ring(s); exterior fault polygons are preserved.\n";
    }

    if (project->getChild<geobo::BOFaultPolygon>(outputName)) {
        NStatus eraseResult = project->eraseChild<geobo::BOFaultPolygon>(outputName);
        if (eraseResult.Value() == NStatus::NFail) {
            throw std::runtime_error("Failed to replace existing BOFaultPolygon: " + outputName);
        }
    }
    std::shared_ptr<geobo::BOFaultPolygon> boPolygon = project->createChild<geobo::BOFaultPolygon>(outputName);
    if (!boPolygon) {
        throw std::runtime_error("Failed to create BOFaultPolygon: " + outputName);
    }
    if (boPolygon->save().Value() == NStatus::NFail) {
        throw std::runtime_error("Failed to save BOFaultPolygon object: " + outputName);
    }

    BAFaultPolygonHead head;
    head.fauNum = 0;
    head.minX = std::numeric_limits<double>::max();
    head.minY = std::numeric_limits<double>::max();
    head.maxX = -std::numeric_limits<double>::max();
    head.maxY = -std::numeric_limits<double>::max();
    head.linestyle = 0;
    head.linewidth = style.lineWidth;
    head.red = style.red;
    head.green = style.green;
    head.blue = style.blue;
    head.linestyle1 = head.linestyle;
    head.linewidth1 = head.linewidth;
    head.red1 = head.red;
    head.green1 = head.green;
    head.blue1 = head.blue;
    head.linestyle2 = head.linestyle;
    head.linewidth2 = head.linewidth;
    head.red2 = head.red;
    head.green2 = head.green;
    head.blue2 = head.blue;
    head.linestyle3 = head.linestyle;
    head.linewidth3 = head.linewidth;
    head.red3 = head.red;
    head.green3 = head.green;
    head.blue3 = head.blue;
    head.linestyle4 = head.linestyle;
    head.linewidth4 = head.linewidth;
    head.red4 = head.red;
    head.green4 = head.green;
    head.blue4 = head.blue;
    head.linestyle5 = head.linestyle;
    head.linewidth5 = head.linewidth;
    head.red5 = head.red;
    head.green5 = head.green;
    head.blue5 = head.blue;

    BAFaultPolygonData data;
    GINT64 nextSegmentId = 1;
    auto appendRingSegment = [&](const std::vector<fpt::Point>& ring) {
        if (ring.size() < 3) {
            return;
        }

        BAFaultPolygonSegment segment;
        segment.editMode = 2;
        segment.faultPolygonSegID = nextSegmentId;
        segment.faultPolygonCombinedID = nextSegmentId;
        segment.pkID = nextSegmentId;
        segment.geometryID = 0;
        segment.type = FA_NORMAL;
        segment.closeFlag = 1;
        segment.level = 0;

        for (const fpt::Point& sourcePoint : ring) {
            BAFaultPolygonPoint point;
            point.x = sourcePoint.x;
            point.y = sourcePoint.y;
            point.z = 0.0;
            point.uplowID = 0;
            segment.faultPolygonData.push_back(point);
            updateBounds(sourcePoint, head.minX, head.minY, head.maxX, head.maxY);
        }

        data.faultPolygonList.push_back(segment);
        ++head.fauNum;
        ++nextSegmentId;
    };

    for (const fpt::Polygon& polygon : polygons) {
        if (keptOnly && !polygon.kept) {
            continue;
        }
        if (polygon.points.size() < 3) {
            continue;
        }

        appendRingSegment(polygon.points);
    }

    if (head.fauNum == 0) {
        head.minX = 0.0;
        head.minY = 0.0;
        head.maxX = 0.0;
        head.maxY = 0.0;
    }

    if (!boPolygon->saveData(head, data)) {
        throw std::runtime_error("Failed to write BOFaultPolygon data: " + outputName);
    }
}

void deleteFaultPolygons(
    const GeoEastProjectPtr& project,
    const std::string& outputName) {
    if (!project) {
        throw std::runtime_error("GeoEast project is null.");
    }
    if (outputName.empty()) {
        throw std::runtime_error("GeoEast BOFaultPolygon output name is empty.");
    }

    if (!project->getChild<geobo::BOFaultPolygon>(outputName)) {
        throw std::runtime_error("BOFaultPolygon does not exist: " + outputName);
    }
    NStatus eraseResult = project->eraseChild<geobo::BOFaultPolygon>(outputName);
    if (eraseResult.Value() == NStatus::NFail) {
        throw std::runtime_error("Failed to delete BOFaultPolygon: " + outputName);
    }
    std::cout << "BOFaultPolygon deleted: " << outputName << "\n";
}

} // namespace fpt_geoeast
