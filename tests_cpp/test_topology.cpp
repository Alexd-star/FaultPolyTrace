#include "../src/core/Topology.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<fpt::Point> square(double x0, double y0, double x1, double y1) {
    return {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
}

void testInteriorHoleIsValid() {
    fpt::Polygon polygon;
    polygon.id = 1;
    polygon.points = square(0.0, 0.0, 10.0, 10.0);
    polygon.holes.push_back(square(3.0, 3.0, 5.0, 5.0));
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({polygon}, true);
    require(diagnostics.selfIntersectingPolygonCount == 0, "An interior hole must remain valid.");
}

void testSinglePointTouchIsValid() {
    fpt::Polygon polygon;
    polygon.id = 2;
    polygon.points = square(0.0, 0.0, 10.0, 10.0);
    polygon.holes.push_back({{0.0, 5.0}, {2.0, 4.0}, {2.0, 6.0}});
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({polygon}, true);
    require(diagnostics.selfIntersectingPolygonCount == 0,
            "A single point touch is valid under the OGC polygon model.");
    require(!fpt::ringBoundariesConflict(polygon.points, polygon.holes.front(), true),
            "A single point touch must not be treated as a disconnected interior.");
}

void testMultiplePointTouchesAreRejected() {
    fpt::Polygon polygon;
    polygon.id = 3;
    polygon.points = square(0.0, 0.0, 12.0, 12.0);
    polygon.holes.push_back({{0.0, 3.0}, {2.0, 3.5}, {2.0, 4.5}, {0.0, 5.0}, {1.0, 4.0}});
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({polygon}, true);
    require(diagnostics.selfIntersectingPolygonCount == 1,
            "Multiple boundary contacts must be rejected as a disconnected interior.");
    require(fpt::ringBoundariesConflict(polygon.points, polygon.holes.front(), true),
            "The ring conflict helper must detect multiple distinct point touches.");
}

} // namespace

int main() {
    try {
        testInteriorHoleIsValid();
        testSinglePointTouchIsValid();
        testMultiplePointTouchesAreRejected();
        std::cout << "Topology tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
