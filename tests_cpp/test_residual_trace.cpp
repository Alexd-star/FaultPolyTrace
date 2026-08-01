#include "../src/core/FaultTracer.h"
#include "../src/core/Topology.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

int keptCount(const fpt::TraceResult& result) {
    int count = 0;
    for (const fpt::Polygon& polygon : result.polygons) {
        count += polygon.kept ? 1 : 0;
    }
    return count;
}

} // namespace

int main() {
    fpt::GridMetadata metadata;
    metadata.nx = 80;
    metadata.ny = 40;
    metadata.dx = 10.0;
    metadata.dy = 10.0;
    fpt::Grid2D grid(metadata);

    for (int x = 5; x <= 30; ++x) {
        grid.value(x, 10) = 1.0;
    }
    for (int x = 45; x <= 60; ++x) {
        grid.value(x, 24 + (x - 45) / 4) = 0.8;
    }

    fpt::TraceOptions options;
    options.traceMode = fpt::TraceMode::Region;
    options.thresholdMode = fpt::ThresholdMode::Percentile;
    options.percentile = 0.997;
    options.minArea = 1.0;
    options.minCells = 1;
    options.weakPercentile = 0.992;
    options.weakConnectRadius = 0;
    options.residualAttributeFill = true;
    options.residualPercentile = 0.992;
    options.residualTrace = false;
    options.residualMinCells = 2;
    options.residualTraceMinLength = 8;
    options.residualTraceMaxCells = 80;
    options.residualTraceDetached = true;
    options.residualDetachedMinCells = 8;
    options.residualDetachedMinLength = 8;
    options.residualDetachedMinAspect = 3.0;
    options.residualDetachedMinMeanRatio = 0.70;
    options.residualDetachedMinMaxRatio = 0.75;
    options.simplifyTolerance = 0.0;
    options.smoothIterations = 0;

    const fpt::TraceResult baseline = fpt::traceFaultPolygons(grid, options);
    assert(keptCount(baseline) == 1);
    assert(baseline.residualTraceCandidateComponents == 0);

    options.residualTrace = true;
    const fpt::TraceResult enhanced = fpt::traceFaultPolygons(grid, options);
    assert(keptCount(enhanced) == 2);
    assert(std::abs(enhanced.residualTraceEffectiveThreshold - 0.8) < 1e-12);
    assert(enhanced.residualTraceAcceptedComponents == 1);
    assert(enhanced.residualTraceAcceptedPolygons == 1);
    assert(enhanced.residualTraceAcceptedCells == 16);
    assert(enhanced.residualTraceTopologyRejectedComponents == 0);
    assert(fpt::topologyIsValid(fpt::diagnoseTopology(enhanced.polygons, true)));

    fpt::Grid2D nonFiniteGrid = grid;
    nonFiniteGrid.value(0, 0) = std::numeric_limits<double>::quiet_NaN();
    bool nonFiniteRejected = false;
    try {
        (void)fpt::traceFaultPolygons(nonFiniteGrid, options);
    } catch (const std::runtime_error&) {
        nonFiniteRejected = true;
    }
    assert(nonFiniteRejected);

    std::cout << "Residual trace integration tests passed.\n";
    return 0;
}
