#include "AsciiGridIO.h"
#include "AtomicOutputFile.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <utility>

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

std::string trimCopy(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

bool equalsIgnoreCase(const std::string& first, const std::string& second) {
    if (first.size() != second.size()) {
        return false;
    }
    for (std::size_t index = 0; index < first.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(first[index])) !=
            std::tolower(static_cast<unsigned char>(second[index]))) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> splitCommaFields(const std::string& value) {
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t comma = value.find(',', start);
        if (comma == std::string::npos) {
            fields.push_back(trimCopy(value.substr(start)));
            break;
        }
        fields.push_back(trimCopy(value.substr(start, comma - start)));
        start = comma + 1;
    }
    return fields;
}

int parseIntegerField(
    const std::string& value,
    std::size_t lineNumber,
    const char* fieldName) {
    std::size_t parsed = 0;
    long long result = 0;
    try {
        result = std::stoll(value, &parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "Invalid " + std::string(fieldName) + " at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }
    if (parsed != value.size() ||
        result < std::numeric_limits<int>::min() ||
        result > std::numeric_limits<int>::max()) {
        throw std::runtime_error(
            "Invalid " + std::string(fieldName) + " at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }
    return static_cast<int>(result);
}

double parseDoubleField(
    const std::string& value,
    std::size_t lineNumber,
    const char* fieldName) {
    std::size_t parsed = 0;
    double result = 0.0;
    try {
        result = std::stod(value, &parsed);
    } catch (const std::exception&) {
        throw std::runtime_error(
            "Invalid " + std::string(fieldName) + " at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }
    if (parsed != value.size() || !std::isfinite(result)) {
        throw std::runtime_error(
            "Invalid " + std::string(fieldName) + " at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }
    return result;
}

bool samePoint(const Point& first, const Point& second) {
    const double scale = std::max({
        1.0,
        std::abs(first.x),
        std::abs(first.y),
        std::abs(second.x),
        std::abs(second.y)});
    return std::abs(first.x - second.x) <= 1e-12 * scale &&
           std::abs(first.y - second.y) <= 1e-12 * scale;
}

std::vector<Point> parseWktRing(
    const std::string& coordinates,
    std::size_t lineNumber) {
    std::vector<Point> points;
    std::size_t start = 0;
    while (start <= coordinates.size()) {
        const std::size_t comma = coordinates.find(',', start);
        const std::string token = trimCopy(coordinates.substr(
            start,
            comma == std::string::npos
                ? std::string::npos
                : comma - start));
        std::istringstream stream(token);
        Point point;
        if (!(stream >> point.x >> point.y) ||
            !std::isfinite(point.x) ||
            !std::isfinite(point.y)) {
            throw std::runtime_error(
                "Invalid POLYGON coordinate at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        stream >> std::ws;
        if (!stream.eof()) {
            throw std::runtime_error(
                "Unexpected POLYGON coordinate token at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        points.push_back(point);
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }

    if (points.size() >= 2 && samePoint(points.front(), points.back())) {
        points.pop_back();
    }
    if (points.size() < 3 || polygonArea(points) <= 0.0) {
        throw std::runtime_error(
            "Degenerate POLYGON ring at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }
    return points;
}

void parsePolygonWkt(
    const std::string& wkt,
    std::size_t lineNumber,
    Polygon& polygon) {
    std::size_t cursor = 0;
    while (cursor < wkt.size() &&
           std::isspace(static_cast<unsigned char>(wkt[cursor]))) {
        ++cursor;
    }
    constexpr char keyword[] = "POLYGON";
    for (std::size_t index = 0; index < sizeof(keyword) - 1; ++index) {
        if (cursor + index >= wkt.size() ||
            std::toupper(static_cast<unsigned char>(wkt[cursor + index])) !=
                keyword[index]) {
            throw std::runtime_error(
                "Expected POLYGON WKT at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
    }
    cursor += sizeof(keyword) - 1;
    while (cursor < wkt.size() &&
           std::isspace(static_cast<unsigned char>(wkt[cursor]))) {
        ++cursor;
    }
    if (cursor >= wkt.size() || wkt[cursor] != '(') {
        throw std::runtime_error(
            "Malformed POLYGON WKT at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }
    ++cursor;

    std::vector<std::vector<Point>> rings;
    while (true) {
        while (cursor < wkt.size() &&
               std::isspace(static_cast<unsigned char>(wkt[cursor]))) {
            ++cursor;
        }
        if (cursor >= wkt.size() || wkt[cursor] != '(') {
            throw std::runtime_error(
                "Malformed POLYGON ring at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        const std::size_t ringStart = ++cursor;
        const std::size_t ringEnd = wkt.find(')', ringStart);
        if (ringEnd == std::string::npos) {
            throw std::runtime_error(
                "Unclosed POLYGON ring at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        rings.push_back(parseWktRing(
            wkt.substr(ringStart, ringEnd - ringStart),
            lineNumber));
        cursor = ringEnd + 1;
        while (cursor < wkt.size() &&
               std::isspace(static_cast<unsigned char>(wkt[cursor]))) {
            ++cursor;
        }
        if (cursor < wkt.size() && wkt[cursor] == ',') {
            ++cursor;
            continue;
        }
        if (cursor >= wkt.size() || wkt[cursor] != ')') {
            throw std::runtime_error(
                "Malformed POLYGON closure at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        ++cursor;
        break;
    }
    while (cursor < wkt.size() &&
           std::isspace(static_cast<unsigned char>(wkt[cursor]))) {
        ++cursor;
    }
    if (cursor != wkt.size() || rings.empty()) {
        throw std::runtime_error(
            "Unexpected text after POLYGON WKT at polygon CSV line " +
            std::to_string(lineNumber) + ".");
    }

    polygon.points = std::move(rings.front());
    polygon.holes.assign(
        std::make_move_iterator(rings.begin() + 1),
        std::make_move_iterator(rings.end()));
}

bool parseGeoEastDatSample(const std::string& textLine, int& line, int& cmp, double& x, double& y, double& value) {
    const char* cursor = textLine.c_str();
    char* end = nullptr;

    errno = 0;
    const long parsedLine = std::strtol(cursor, &end, 10);
    if (end == cursor || errno == ERANGE || parsedLine < INT_MIN || parsedLine > INT_MAX) {
        return false;
    }
    line = static_cast<int>(parsedLine);
    cursor = end;

    errno = 0;
    const long parsedCmp = std::strtol(cursor, &end, 10);
    if (end == cursor || errno == ERANGE || parsedCmp < INT_MIN || parsedCmp > INT_MAX) {
        return false;
    }
    cmp = static_cast<int>(parsedCmp);
    cursor = end;

    errno = 0;
    x = std::strtod(cursor, &end);
    if (end == cursor || errno == ERANGE || !std::isfinite(x)) {
        return false;
    }
    cursor = end;

    errno = 0;
    y = std::strtod(cursor, &end);
    if (end == cursor || errno == ERANGE || !std::isfinite(y)) {
        return false;
    }
    cursor = end;

    errno = 0;
    value = std::strtod(cursor, &end);
    if (end == cursor || errno == ERANGE || !std::isfinite(value)) {
        return false;
    }
    cursor = end;
    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor))) {
        ++cursor;
    }
    return *cursor == '\0';
}

template <typename Map>
void rememberCoordinate(
    Map& coordinateByKey,
    std::vector<int>& keys,
    int key,
    double coordinate,
    const char* axisName) {
    const auto inserted = coordinateByKey.emplace(key, coordinate);
    if (inserted.second) {
        keys.push_back(key);
        return;
    }
    const double scale = std::max(
        1.0,
        std::max(std::abs(inserted.first->second), std::abs(coordinate)));
    if (std::abs(inserted.first->second - coordinate) > 1e-9 * scale) {
        throw std::runtime_error(
            std::string("GeoEast DAT contains inconsistent ") + axisName +
            " coordinates for index " + std::to_string(key) + ".");
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
    std::size_t lineNumber = 0;
    while (std::getline(in, textLine)) {
        ++lineNumber;
        const std::size_t firstNonSpace = textLine.find_first_not_of(" \t\r\n");
        if (firstNonSpace == std::string::npos || textLine[firstNonSpace] == '#') {
            continue;
        }

        int line = 0;
        int cmp = 0;
        double x = 0.0;
        double y = 0.0;
        double value = 0.0;
        if (!parseGeoEastDatSample(textLine, line, cmp, x, y, value)) {
            throw std::runtime_error(
                "Invalid GeoEast DAT sample at line " +
                std::to_string(lineNumber) + ".");
        }
        samples.push_back({line, cmp, value});
        rememberCoordinate(lineToX, lineKeys, line, x, "X");
        rememberCoordinate(cmpToY, cmpKeys, cmp, y, "Y");
    }

    if (samples.empty()) {
        throw std::runtime_error("GeoEast DAT grid contains no samples: " + path);
    }

    std::sort(lineKeys.begin(), lineKeys.end());
    std::sort(cmpKeys.begin(), cmpKeys.end());

    const int nx = static_cast<int>(lineKeys.size());
    const int ny = static_cast<int>(cmpKeys.size());
    const std::size_t expectedSamples =
        static_cast<std::size_t>(nx) * static_cast<std::size_t>(ny);
    if (samples.size() != expectedSamples) {
        throw std::runtime_error(
            "GeoEast DAT grid is incomplete: expected " +
            std::to_string(expectedSamples) + " samples, got " +
            std::to_string(samples.size()) + ".");
    }
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
    std::vector<unsigned char> seenSamples(grid.size(), 0);
    for (const Sample& sample : samples) {
        const int xIndex = lineIndex.at(sample.line);
        const int yIndex = cmpIndex.at(sample.cmp);
        const std::size_t flatIndex =
            static_cast<std::size_t>(yIndex) * static_cast<std::size_t>(nx) +
            static_cast<std::size_t>(xIndex);
        if (seenSamples[flatIndex] != 0) {
            throw std::runtime_error(
                "GeoEast DAT grid contains a duplicate Line/CMP sample.");
        }
        seenSamples[flatIndex] = 1;
        grid.value(xIndex, yIndex) = sample.value;
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
            if (!std::isfinite(grid.value(x, y))) {
                throw std::runtime_error(
                    "Grid file contains a non-finite attribute value.");
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

std::vector<Polygon> readKeptPolygonCsv(const std::string& path) {
    std::ifstream in(filesystemPath(path));
    if (!in) {
        throw std::runtime_error("Failed to open polygon CSV file: " + path);
    }

    std::string header;
    if (!std::getline(in, header)) {
        throw std::runtime_error("Polygon CSV file is empty: " + path);
    }
    if (!header.empty() && header.back() == '\r') {
        header.pop_back();
    }
    const std::vector<std::string> headerFields = splitCommaFields(header);
    const std::vector<std::string> expectedHeader{
        "id", "status", "area", "perimeter", "cell_count",
        "mean_value", "max_value", "wkt"};
    if (headerFields.size() != expectedHeader.size()) {
        throw std::runtime_error(
            "Polygon CSV header does not match the FaultPolyTrace schema: " + path);
    }
    for (std::size_t index = 0; index < expectedHeader.size(); ++index) {
        if (!equalsIgnoreCase(headerFields[index], expectedHeader[index])) {
            throw std::runtime_error(
                "Polygon CSV header does not match the FaultPolyTrace schema: " + path);
        }
    }

    std::vector<Polygon> polygons;
    std::set<int> keptIds;
    std::string line;
    std::size_t lineNumber = 1;
    while (std::getline(in, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (trimCopy(line).empty()) {
            continue;
        }

        const std::size_t firstQuote = line.find('"');
        const std::size_t lastQuote = line.rfind('"');
        if (firstQuote == std::string::npos ||
            lastQuote == firstQuote ||
            !trimCopy(line.substr(lastQuote + 1)).empty()) {
            throw std::runtime_error(
                "Malformed quoted WKT at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        std::string prefix = line.substr(0, firstQuote);
        if (prefix.empty() || prefix.back() != ',') {
            throw std::runtime_error(
                "Malformed polygon CSV fields at line " +
                std::to_string(lineNumber) + ".");
        }
        prefix.pop_back();
        const std::vector<std::string> fields = splitCommaFields(prefix);
        if (fields.size() != 7) {
            throw std::runtime_error(
                "Expected seven polygon fields before WKT at line " +
                std::to_string(lineNumber) + ".");
        }
        if (!equalsIgnoreCase(fields[1], "kept") &&
            !equalsIgnoreCase(fields[1], "filtered")) {
            throw std::runtime_error(
                "Unknown polygon status at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        if (equalsIgnoreCase(fields[1], "filtered")) {
            continue;
        }

        Polygon polygon;
        polygon.id = parseIntegerField(fields[0], lineNumber, "polygon id");
        if (!keptIds.insert(polygon.id).second) {
            throw std::runtime_error(
                "Duplicate kept polygon id at polygon CSV line " +
                std::to_string(lineNumber) + ".");
        }
        polygon.area = parseDoubleField(fields[2], lineNumber, "polygon area");
        polygon.perimeter = parseDoubleField(fields[3], lineNumber, "polygon perimeter");
        polygon.cellCount = parseIntegerField(fields[4], lineNumber, "cell count");
        polygon.meanValue = parseDoubleField(fields[5], lineNumber, "mean value");
        polygon.maxValue = parseDoubleField(fields[6], lineNumber, "max value");
        polygon.kept = true;
        parsePolygonWkt(
            line.substr(firstQuote + 1, lastQuote - firstQuote - 1),
            lineNumber,
            polygon);
        polygons.push_back(std::move(polygon));
    }
    if (polygons.empty()) {
        throw std::runtime_error("Polygon CSV contains no kept polygons: " + path);
    }
    return polygons;
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
