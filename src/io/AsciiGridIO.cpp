#include "AsciiGridIO.h"
#include "AtomicOutputFile.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace fpt {
namespace {

std::filesystem::path filesystemPath(const std::string& utf8Path) {
#ifdef _WIN32
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                           utf8Path.data(), static_cast<int>(utf8Path.size()),
                                           nullptr, 0);
    if (length <= 0) {
        throw std::runtime_error("Invalid UTF-8 file path: " + utf8Path);
    }
    std::wstring wide(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                        utf8Path.data(), static_cast<int>(utf8Path.size()),
                        wide.data(), length);
    return std::filesystem::path(wide);
#else
    return std::filesystem::path(utf8Path);
#endif
}

void writeRingWkt(std::ostream& out, const std::vector<Point>& points) {
    out << "(";
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << points[i].x << " " << points[i].y;
    }
    if (!points.empty()) {
        out << "," << points.front().x << " " << points.front().y;
    }
    out << ")";
}

void writePolygonWkt(std::ostream& out, const Polygon& polygon) {
    out << "POLYGON(";
    writeRingWkt(out, polygon.points);
    for (const std::vector<Point>& hole : polygon.holes) {
        if (hole.size() < 3) {
            continue;
        }
        out << ",";
        writeRingWkt(out, hole);
    }
    out << ")";
}

void writeLineStringWkt(std::ostream& out, const std::vector<Point>& points) {
    out << "LINESTRING(";
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << points[i].x << " " << points[i].y;
    }
    out << ")";
}

bool parseGeoEastDatSample(const std::string& textLine, int& line, int& cmp, double& x, double& y, double& value) {
    const char* cursor = textLine.c_str();
    char* end = nullptr;

    line = static_cast<int>(std::strtol(cursor, &end, 10));
    if (end == cursor) {
        return false;
    }
    cursor = end;

    cmp = static_cast<int>(std::strtol(cursor, &end, 10));
    if (end == cursor) {
        return false;
    }
    cursor = end;

    x = std::strtod(cursor, &end);
    if (end == cursor) {
        return false;
    }
    cursor = end;

    y = std::strtod(cursor, &end);
    if (end == cursor) {
        return false;
    }
    cursor = end;

    value = std::strtod(cursor, &end);
    return end != cursor;
}

template <typename Map>
void rememberCoordinate(Map& coordinateByKey, std::vector<int>& keys, int key, double coordinate) {
    if (coordinateByKey.emplace(key, coordinate).second) {
        keys.push_back(key);
    }
}

} // namespace

Grid2D readGeoEastDatGrid(const std::string& path) {
    std::vector<char> inputBuffer(1024 * 1024);
    std::ifstream in;
    in.rdbuf()->pubsetbuf(inputBuffer.data(), static_cast<std::streamsize>(inputBuffer.size()));
    in.open(filesystemPath(path));
    if (!in) {
        throw std::runtime_error("Failed to open GeoEast DAT grid file: " + path);
    }

    struct Sample {
        int line = 0;
        int cmp = 0;
        double value = 0.0;
    };

    std::vector<Sample> samples;
    in.seekg(0, std::ios::end);
    const std::streamoff inputBytes = in.tellg();
    in.seekg(0, std::ios::beg);
    if (inputBytes > 0) {
        const auto estimatedSamples = static_cast<std::size_t>(inputBytes / 64) + 1;
        samples.reserve(std::min<std::size_t>(estimatedSamples, 16 * 1024 * 1024));
    } else {
        samples.reserve(1024 * 1024);
    }
    std::unordered_map<int, double> lineToX;
    std::unordered_map<int, double> cmpToY;
    std::vector<int> lineKeys;
    std::vector<int> cmpKeys;
    lineToX.reserve(4096);
    cmpToY.reserve(4096);
    lineKeys.reserve(4096);
    cmpKeys.reserve(4096);

    std::string textLine;
    while (std::getline(in, textLine)) {
        if (textLine.empty() || textLine[0] == '#') {
            continue;
        }

        int line = 0;
        int cmp = 0;
        double x = 0.0;
        double y = 0.0;
        double value = 0.0;
        if (!parseGeoEastDatSample(textLine, line, cmp, x, y, value)) {
            continue;
        }
        samples.push_back({line, cmp, value});
        rememberCoordinate(lineToX, lineKeys, line, x);
        rememberCoordinate(cmpToY, cmpKeys, cmp, y);
    }

    if (samples.empty()) {
        throw std::runtime_error("GeoEast DAT grid contains no samples: " + path);
    }

    std::sort(lineKeys.begin(), lineKeys.end());
    std::sort(cmpKeys.begin(), cmpKeys.end());

    const int nx = static_cast<int>(lineKeys.size());
    const int ny = static_cast<int>(cmpKeys.size());
    GridMetadata metadata;
    metadata.nx = nx;
    metadata.ny = ny;

    if (nx > 1) {
        metadata.dx = std::abs(lineToX.at(lineKeys[1]) - lineToX.at(lineKeys[0]));
    }
    if (ny > 1) {
        metadata.dy = std::abs(cmpToY.at(cmpKeys[1]) - cmpToY.at(cmpKeys[0]));
    }

    // GeoEast DAT rows store sample-center coordinates. Polygon rings describe
    // cell boundaries, so shift the grid origin by half a sample interval.
    metadata.sx = lineToX.at(lineKeys.front()) - 0.5 * metadata.dx;
    metadata.sy = cmpToY.at(cmpKeys.front()) - 0.5 * metadata.dy;

    std::unordered_map<int, int> lineIndex;
    std::unordered_map<int, int> cmpIndex;
    lineIndex.reserve(lineKeys.size() * 2);
    cmpIndex.reserve(cmpKeys.size() * 2);
    for (std::size_t i = 0; i < lineKeys.size(); ++i) {
        lineIndex.emplace(lineKeys[i], static_cast<int>(i));
    }
    for (std::size_t i = 0; i < cmpKeys.size(); ++i) {
        cmpIndex.emplace(cmpKeys[i], static_cast<int>(i));
    }

    Grid2D grid(metadata);
    std::fill(grid.values().begin(), grid.values().end(), 0.0);
    for (const Sample& sample : samples) {
        grid.value(lineIndex.at(sample.line), cmpIndex.at(sample.cmp)) = sample.value;
    }
    return grid;
}

Grid2D readAsciiGrid(const std::string& path) {
    std::ifstream in(filesystemPath(path));
    if (!in) {
        throw std::runtime_error("Failed to open grid file: " + path);
    }

    GridMetadata metadata;
    if (!(in >> metadata.nx >> metadata.ny)) {
        throw std::runtime_error("Failed to read grid dimensions.");
    }
    if (!(in >> metadata.sx >> metadata.sy)) {
        throw std::runtime_error("Failed to read grid origin.");
    }
    if (!(in >> metadata.dx >> metadata.dy)) {
        throw std::runtime_error("Failed to read grid spacing.");
    }

    Grid2D grid(metadata);
    for (int y = 0; y < metadata.ny; ++y) {
        for (int x = 0; x < metadata.nx; ++x) {
            if (!(in >> grid.value(x, y))) {
                throw std::runtime_error("Grid file ended before all values were read.");
            }
        }
    }
    return grid;
}

Grid2D readAutoGrid(const std::string& path) {
    std::ifstream in(filesystemPath(path));
    if (!in) {
        throw std::runtime_error("Failed to open grid file: " + path);
    }

    std::string firstLine;
    while (std::getline(in, firstLine)) {
        if (!firstLine.empty()) {
            break;
        }
    }
    if (firstLine.find("Line") != std::string::npos &&
        firstLine.find("CMP") != std::string::npos &&
        firstLine.find("Value") != std::string::npos) {
        return readGeoEastDatGrid(path);
    }
    return readAsciiGrid(path);
}

void writePolygonCsv(const std::string& path, const std::vector<Polygon>& polygons) {
    AtomicOutputFile output(filesystemPath(path), 1024 * 1024);
    std::ostream& out = output.stream();

    out << "id,status,area,perimeter,cell_count,mean_value,max_value,wkt\n";
    out << std::fixed << std::setprecision(6);
    for (const Polygon& polygon : polygons) {
        out << polygon.id << ','
            << (polygon.kept ? "kept" : "filtered") << ','
            << polygon.area << ','
            << polygon.perimeter << ','
            << polygon.cellCount << ','
            << polygon.meanValue << ','
            << polygon.maxValue << ','
            << '"';
        writePolygonWkt(out, polygon);
        out << '"' << '\n';
    }
    output.commit();
}

void writeBranchCsv(const std::string& path, const std::vector<BranchTrace>& branches) {
    AtomicOutputFile output(filesystemPath(path), 1024 * 1024);
    std::ostream& out = output.stream();

    out << "id,status,point_count,length,mean_value,max_value,start_is_junction,end_is_junction,wkt\n";
    out << std::fixed << std::setprecision(6);
    for (const BranchTrace& branch : branches) {
        out << branch.id << ','
            << (branch.kept ? "kept" : "pruned") << ','
            << branch.pointCount << ','
            << branch.length << ','
            << branch.meanValue << ','
            << branch.maxValue << ','
            << (branch.startIsJunction ? 1 : 0) << ','
            << (branch.endIsJunction ? 1 : 0) << ','
            << '"';
        writeLineStringWkt(out, branch.points);
        out << '"' << '\n';
    }
    output.commit();
}

} // namespace fpt
