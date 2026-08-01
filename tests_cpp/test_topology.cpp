#include "../src/core/Topology.h"

#include <cstdlib>
#include <iostream>
#include <limits>
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
    require(fpt::topologyIsValid(diagnostics), "A regular polygon with an interior hole must be valid.");
}

void testRepeatedClosingVertexIsRejected() {
    fpt::Polygon polygon;
    polygon.id = 4;
    polygon.points = square(0.0, 0.0, 10.0, 10.0);
    polygon.points.push_back(polygon.points.front());
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({polygon}, true);
    require(diagnostics.invalidPolygonCount == 1,
            "The internal ring model must reject a duplicated closing vertex.");
    require(diagnostics.zeroLengthEdgeCount == 1,
            "A duplicated closing vertex must expose its zero-length edge.");
    require(diagnostics.duplicateVertexRingCount == 1,
            "A duplicated closing vertex must be counted as a duplicate ring vertex.");
}

void testNonFiniteAndZeroAreaRingsAreRejected() {
    fpt::Polygon nonFinite;
    nonFinite.id = 5;
    nonFinite.points = square(0.0, 0.0, 10.0, 10.0);
    nonFinite.points[2].x = std::numeric_limits<double>::quiet_NaN();
    fpt::Polygon zeroArea;
    zeroArea.id = 6;
    zeroArea.points = {{0.0, 0.0}, {5.0, 0.0}, {10.0, 0.0}};
    const fpt::TopologyDiagnostics diagnostics =
        fpt::diagnoseTopology({nonFinite, zeroArea}, true);
    require(diagnostics.invalidPolygonCount == 2,
            "Non-finite and zero-area rings must both be invalid.");
    require(diagnostics.nonFiniteCoordinateCount == 1,
            "The non-finite coordinate must be reported.");
    require(diagnostics.degenerateRingCount == 1,
            "The zero-area ring must be reported as degenerate.");
}

void testHoleOutsideShellIsRejected() {
    fpt::Polygon polygon;
    polygon.id = 7;
    polygon.points = square(0.0, 0.0, 10.0, 10.0);
    polygon.holes.push_back(square(20.0, 20.0, 22.0, 22.0));
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({polygon}, true);
    require(diagnostics.invalidPolygonCount == 1,
            "A hole outside its exterior shell must be invalid.");
    require(diagnostics.holeOutsideShellCount == 1,
            "The outside-hole reason must be reported.");
}

void testNestedHolesAreRejected() {
    fpt::Polygon polygon;
    polygon.id = 8;
    polygon.points = square(0.0, 0.0, 20.0, 20.0);
    polygon.holes.push_back(square(2.0, 2.0, 18.0, 18.0));
    polygon.holes.push_back(square(5.0, 5.0, 10.0, 10.0));
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({polygon}, true);
    require(diagnostics.invalidPolygonCount == 1,
            "Nested holes in one polygon must be invalid.");
    require(diagnostics.nestedHoleCount == 1,
            "Nested-hole relationships must be reported.");
}

void testCompletePolygonContainmentIsDetected() {
    fpt::Polygon outer;
    outer.id = 9;
    outer.points = square(0.0, 0.0, 20.0, 20.0);
    fpt::Polygon inner;
    inner.id = 10;
    inner.points = square(5.0, 5.0, 10.0, 10.0);
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({outer, inner}, true);
    require(diagnostics.polygonIntersectionCount == 1,
            "Complete containment must be treated as a polygon conflict.");
    require(diagnostics.polygonContainmentCount == 1,
            "Complete containment must have an explicit diagnostic count.");
    require(diagnostics.polygonInteriorOverlapCount == 1,
            "Complete containment must be recognized as interior-area overlap.");
    require(!fpt::topologyIsValid(diagnostics),
            "A polygon set with overlapping containment must fail validity.");
}

void testPolygonInsideAnotherHoleDoesNotOverlap() {
    fpt::Polygon frame;
    frame.id = 11;
    frame.points = square(0.0, 0.0, 20.0, 20.0);
    frame.holes.push_back(square(4.0, 4.0, 16.0, 16.0));
    fpt::Polygon island;
    island.id = 12;
    island.points = square(6.0, 6.0, 10.0, 10.0);
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({frame, island}, true);
    require(diagnostics.polygonIntersectionCount == 0,
            "A polygon wholly inside another polygon's hole has no area overlap.");
    require(fpt::topologyIsValid(diagnostics),
            "A separate polygon inside a hole must pass set-level validity.");
}

void testCrossingPolygonInteriorsAreDetected() {
    fpt::Polygon first;
    first.id = 13;
    first.points = square(0.0, 0.0, 10.0, 10.0);
    fpt::Polygon second;
    second.id = 14;
    second.points = square(5.0, 5.0, 15.0, 15.0);
    const fpt::TopologyDiagnostics diagnostics = fpt::diagnoseTopology({first, second}, true);
    require(diagnostics.polygonIntersectionCount == 1,
            "Crossing polygon boundaries must be reported once per polygon pair.");
    require(diagnostics.polygonInteriorOverlapCount == 1,
            "Proper boundary crossings must indicate interior overlap.");
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
        testRepeatedClosingVertexIsRejected();
        testNonFiniteAndZeroAreaRingsAreRejected();
        testHoleOutsideShellIsRejected();
        testNestedHolesAreRejected();
        testCompletePolygonContainmentIsDetected();
        testPolygonInsideAnotherHoleDoesNotOverlap();
        testCrossingPolygonInteriorsAreDetected();
        std::cout << "Topology tests passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
