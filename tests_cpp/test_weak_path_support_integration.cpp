#include "../src/core/FaultTracer.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeWeakCorridorGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 40;
    metadata.ny = 15;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 0.0;
    }
    for (int x = 2; x <= 5; ++x) {
        grid.value(x, 7) = 1.0;
    }
    for (int x = 6; x <= 19; ++x) {
        grid.value(x, 7) = 0.5;
    }
    for (int x = 20; x <= 28; ++x) {
        grid.value(x, 7) = 0.90;
    }
    return grid;
}

fpt::Grid2D makeLegalShortGapGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 28;
    metadata.ny = 15;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 0.0;
    }
    for (int x = 2; x <= 5; ++x) {
        grid.value(x, 7) = 1.0;
    }
    for (int x = 6; x <= 7; ++x) {
        grid.value(x, 7) = 0.50;
    }
    for (int x = 8; x <= 12; ++x) {
        grid.value(x, 7) = 0.90;
    }
    for (int x = 15; x <= 24; ++x) {
        grid.value(x, 1) = 0.50;
    }
    return grid;
}

fpt::Grid2D makeDetachedWeakLobeGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 36;
    metadata.ny = 15;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 0.0;
    }
    for (int x = 2; x <= 5; ++x) {
        grid.value(x, 7) = 1.0;
    }
    for (int x = 22; x <= 30; ++x) {
        grid.value(x, 7) = 0.90;
    }
    for (int x = 10; x <= 19; ++x) {
        grid.value(x, 1) = 0.50;
    }
    return grid;
}

fpt::Grid2D makeWideWeakBlobGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 25;
    metadata.ny = 25;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 0.0;
    }
    for (int x = 2; x <= 4; ++x) {
        grid.value(x, 12) = 1.0;
    }
    for (int y = 6; y <= 17; ++y) {
        for (int x = 5; x <= 16; ++x) {
            grid.value(x, y) = 0.90;
        }
    }
    return grid;
}

fpt::Grid2D makeWeakContinuityLandingGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 48;
    metadata.ny = 24;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 0.0;
    }
    for (int x = 4; x <= 14; ++x) {
        grid.value(x, 12) = 1.0;
    }
    for (int x = 15; x <= 26; ++x) {
        grid.value(x, 12) = 0.60;
    }
    for (int x = 27; x <= 40; ++x) {
        grid.value(x, 12) = 1.0;
    }
    return grid;
}

void setGridLine(
    fpt::Grid2D& grid,
    int x0,
    int y0,
    int x1,
    int y1,
    double value) {
    const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int step = 0; step <= steps; ++step) {
        const double fraction = steps > 0
            ? static_cast<double>(step) / static_cast<double>(steps)
            : 0.0;
        const int x = static_cast<int>(std::lround(
            x0 + fraction * static_cast<double>(x1 - x0)));
        const int y = static_cast<int>(std::lround(
            y0 + fraction * static_cast<double>(y1 - y0)));
        grid.value(x, y) = value;
    }
}

fpt::Grid2D makeThreeSegmentFinalBridgeGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 64;
    metadata.ny = 32;
    fpt::Grid2D grid(metadata);
    std::fill(grid.values().begin(), grid.values().end(), 0.0);
    setGridLine(grid, 4, 16, 14, 16, 1.0);
    setGridLine(grid, 15, 16, 19, 16, 0.65);
    setGridLine(grid, 20, 16, 30, 16, 1.0);
    setGridLine(grid, 31, 16, 35, 16, 0.65);
    setGridLine(grid, 36, 16, 52, 16, 1.0);
    return grid;
}

fpt::Grid2D makeAmbiguousJunctionFinalBridgeGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 56;
    metadata.ny = 36;
    fpt::Grid2D grid(metadata);
    std::fill(grid.values().begin(), grid.values().end(), 0.0);
    setGridLine(grid, 10, 8, 17, 15, 1.0);
    setGridLine(grid, 10, 22, 17, 15, 1.0);
    setGridLine(grid, 17, 15, 24, 15, 1.0);
    setGridLine(grid, 25, 15, 29, 15, 0.65);
    setGridLine(grid, 30, 15, 45, 15, 1.0);
    return grid;
}

fpt::Grid2D makeNearbyThirdTraceFinalBridgeGrid() {
    fpt::GridMetadata metadata;
    metadata.nx = 60;
    metadata.ny = 36;
    fpt::Grid2D grid(metadata);
    std::fill(grid.values().begin(), grid.values().end(), 0.0);
    setGridLine(grid, 5, 18, 16, 18, 1.0);
    setGridLine(grid, 17, 18, 21, 18, 0.65);
    setGridLine(grid, 22, 18, 40, 18, 1.0);
    setGridLine(grid, 19, 14, 28, 5, 1.0);
    return grid;
}

int keptPolygonCount(const fpt::TraceResult& result) {
    int count = 0;
    for (const fpt::Polygon& polygon : result.polygons) {
        count += polygon.kept ? 1 : 0;
    }
    return count;
}

fpt::TraceOptions baseOptions() {
    fpt::TraceOptions options;
    options.traceMode = fpt::TraceMode::Region;
    options.thresholdMode = fpt::ThresholdMode::Fixed;
    options.threshold = 0.95;
    options.weakPercentile = 0.97;
    options.weakConnectRadius = 1;
    options.maxWeakComponentCells = 8;
    options.minWeakComponentAspect = 2.0;
    options.minArea = 0.0;
    options.minCells = 1;
    options.openRadius = 0;
    options.closeRadius = 0;
    options.simplifyTolerance = 0.0;
    options.smoothIterations = 0;
    return options;
}

fpt::TraceOptions finalObjectBridgeOptions() {
    fpt::TraceOptions options;
    options.traceMode = fpt::TraceMode::RidgeBelt;
    options.thresholdMode = fpt::ThresholdMode::Fixed;
    options.threshold = 0.90;
    options.weakPercentile = 0.0;
    options.beltRadius = 2;
    options.attributeConstrainedBelt = true;
    options.attributeEnvelopeRadius = 1;
    options.postBeltCloseRadius = 0;
    options.minBranchLength = 2;
    options.minJunctionBranchLength = 2;
    options.minJunctionBranchMean = 0.0;
    options.maxGapDistance = 0;
    options.residualAttributeFill = false;
    options.minArea = 0.0;
    options.minCells = 1;
    options.simplifyTolerance = 0.0;
    options.smoothIterations = 0;
    options.weakContinuityFinalObjectPass = true;
    options.weakContinuityMaxDistance = 12;
    options.weakContinuityTangentWindow = 5;
    options.weakContinuityCorridorRadius = 4;
    options.weakContinuityMaxCandidatesPerEndpoint = 3;
    options.weakContinuityMaxEndpointAngle = 35.0;
    options.weakContinuityMinAttributeRatio = 0.40;
    options.weakContinuityMinSupportRatio = 0.65;
    options.weakContinuityMinMeanSupport = 0.52;
    options.weakContinuityMaxUnsupportedRun = 1;
    options.weakContinuityMaxPathLengthRatio = 1.20;
    options.weakContinuityMaxTurnAngle = 30.0;
    options.weakContinuityMinCompetitionMargin = 0.0;
    return options;
}

void testExperimentalGateReducesUnsupportedExtension() {
    const fpt::Grid2D grid = makeWeakCorridorGrid();
    fpt::TraceOptions legacy = baseOptions();
    const fpt::TraceResult legacyResult = fpt::traceFaultPolygons(grid, legacy);

    fpt::TraceOptions gated = baseOptions();
    gated.weakPathGate = true;
    gated.weakPathSupportRatio = 0.50;
    gated.weakPathMaxUnsupportedRun = 3;
    const fpt::TraceResult gatedResult = fpt::traceFaultPolygons(grid, gated);

    require(
        legacyResult.activeCellCount > gatedResult.activeCellCount,
        "Experimental path gate should reject unsupported remote extension.");
    require(
        gatedResult.weakPathRejectedCells > 0,
        "Rejected weak-path cells should be reported.");
    require(
        gatedResult.weakPathAcceptedCells > 0,
        "Supported weak-path cells should remain accepted.");
}

void testLegalShortGapStillKeepsRemoteWeakExtension() {
    const fpt::Grid2D grid = makeLegalShortGapGrid();
    fpt::TraceOptions strongOnly = baseOptions();
    strongOnly.weakPercentile = 0.0;
    const fpt::TraceResult strongOnlyResult =
        fpt::traceFaultPolygons(grid, strongOnly);

    fpt::TraceOptions gated = baseOptions();
    gated.weakPathGate = true;
    gated.weakPathSupportRatio = 0.50;
    gated.weakPathMaxUnsupportedRun = 2;
    const fpt::TraceResult gatedResult =
        fpt::traceFaultPolygons(grid, gated);

    require(
        gatedResult.activeCellCount > strongOnlyResult.activeCellCount,
        "A legal short gap should retain the supported remote weak extension.");
    require(
        gatedResult.weakPathAcceptedCells > 2,
        "Accepted path evidence should extend beyond the short gap itself.");
}

void testDetachedWeakEvidenceFallsBackToStrongResult() {
    const fpt::Grid2D grid = makeDetachedWeakLobeGrid();
    fpt::TraceOptions strongOnly = baseOptions();
    strongOnly.weakPercentile = 0.0;
    const fpt::TraceResult strongOnlyResult =
        fpt::traceFaultPolygons(grid, strongOnly);

    fpt::TraceOptions gated = baseOptions();
    gated.weakPathGate = true;
    const fpt::TraceResult gatedResult =
        fpt::traceFaultPolygons(grid, gated);

    require(
        gatedResult.activeCellCount == strongOnlyResult.activeCellCount,
        "Detached weak evidence must fall back to the original strong response.");
    require(
        gatedResult.weakPathAcceptedCells == 0,
        "Detached weak evidence must not be promoted.");
}

void testWideWeakBlobIsRejectedByEndToEndEvidenceGate() {
    const fpt::Grid2D grid = makeWideWeakBlobGrid();
    fpt::TraceOptions legacy = baseOptions();
    legacy.maxWeakComponentCells = 1000;
    const fpt::TraceResult legacyResult =
        fpt::traceFaultPolygons(grid, legacy);

    fpt::TraceOptions gated = legacy;
    gated.weakPathGate = true;
    gated.weakPathSupportRatio = 0.0;
    const fpt::TraceResult gatedResult =
        fpt::traceFaultPolygons(grid, gated);

    require(
        gatedResult.activeCellCount < legacyResult.activeCellCount,
        "The end-to-end evidence gate should reject an attached wide weak blob.");
    require(
        gatedResult.weakPathShapeRejectedFragments > 0,
        "Wide weak-blob rejection should be visible in trace diagnostics.");
}

void testDefaultGateRemainsDisabled() {
    const fpt::TraceOptions options = baseOptions();
    require(!options.weakPathGate, "Weak path gate must remain disabled by default.");
}

void testNonFiniteSupportRatioIsRejected() {
    fpt::TraceOptions options = baseOptions();
    options.weakPathSupportRatio = std::numeric_limits<double>::quiet_NaN();

    bool rejected = false;
    try {
        (void)fpt::traceFaultPolygons(makeWeakCorridorGrid(), options);
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    require(rejected, "Non-finite weak path support ratio must be rejected.");
}

void testCandidateRootGroupingRejectsUnsupportedCoreModes() {
    const fpt::Grid2D grid = makeLegalShortGapGrid();

    const auto requireRejected = [&](fpt::TraceOptions options) {
        bool rejected = false;
        try {
            (void)fpt::traceFaultPolygons(grid, options);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected,
                "Candidate root grouping must reject unsupported core option combinations.");
    };

    fpt::TraceOptions region = baseOptions();
    region.branchAttributionCandidateGroups = true;
    region.rootAntiMerge = true;
    requireRejected(region);

    fpt::TraceOptions missingAntiMerge = baseOptions();
    missingAntiMerge.traceMode = fpt::TraceMode::RidgeBelt;
    missingAntiMerge.branchAttributionCandidateGroups = true;
    missingAntiMerge.rootAntiMerge = false;
    requireRejected(missingAntiMerge);

    fpt::TraceOptions separateBelts = baseOptions();
    separateBelts.traceMode = fpt::TraceMode::RidgeBelt;
    separateBelts.branchAttributionCandidateGroups = true;
    separateBelts.rootAntiMerge = true;
    separateBelts.separateBranchBelts = true;
    requireRejected(separateBelts);
}

void testAcceptedWeakContinuitySurvivesFinalPolygonization() {
    const fpt::Grid2D grid = makeWeakContinuityLandingGrid();
    fpt::TraceOptions baseline;
    baseline.traceMode = fpt::TraceMode::RidgeBelt;
    baseline.thresholdMode = fpt::ThresholdMode::Fixed;
    baseline.threshold = 0.90;
    baseline.weakPercentile = 0.0;
    baseline.beltRadius = 2;
    baseline.attributeConstrainedBelt = true;
    baseline.attributeEnvelopeRadius = 1;
    baseline.postBeltCloseRadius = 1;
    baseline.minBranchLength = 2;
    baseline.minJunctionBranchLength = 2;
    baseline.minJunctionBranchMean = 0.0;
    baseline.maxGapDistance = 0;
    baseline.rootAntiMerge = true;
    baseline.residualAttributeFill = false;
    baseline.minArea = 0.0;
    baseline.minCells = 1;
    baseline.simplifyTolerance = 0.0;
    baseline.smoothIterations = 0;

    const fpt::TraceResult baselineResult =
        fpt::traceFaultPolygons(grid, baseline);
    require(
        keptPolygonCount(baselineResult) == 2,
        "The unconnected synthetic traces should form two baseline polygons.");

    fpt::TraceOptions candidate = baseline;
    candidate.weakContinuity = true;
    candidate.weakContinuityMaxDistance = 20;
    candidate.weakContinuityTangentWindow = 4;
    candidate.weakContinuityCorridorRadius = 4;
    candidate.weakContinuityMaxCandidatesPerEndpoint = 2;
    candidate.weakContinuityMaxEndpointAngle = 25.0;
    candidate.weakContinuityMinAttributeRatio = 0.40;
    candidate.weakContinuityMinSupportRatio = 0.60;
    candidate.weakContinuityMinMeanSupport = 0.50;
    candidate.weakContinuityMaxUnsupportedRun = 0;
    candidate.weakContinuityMaxPathLengthRatio = 1.20;
    candidate.weakContinuityMaxTurnAngle = 30.0;
    candidate.weakContinuityMinCompetitionMargin = 0.0;
    const fpt::TraceResult candidateResult =
        fpt::traceFaultPolygons(grid, candidate);

    require(
        candidateResult.weakContinuityStats.acceptedConnections == 1,
        "The synthetic supported weak path should be accepted once.");
    require(
        keptPolygonCount(candidateResult) == 1,
        "An accepted weak path must survive the envelope, anti-merge, and polygonization stages.");
}

void testFinalObjectPassConnectsThreeSupportedCollinearSegments() {
    const fpt::TraceResult result = fpt::traceFaultPolygons(
        makeThreeSegmentFinalBridgeGrid(), finalObjectBridgeOptions());
    require(
        result.weakContinuityFinalObjectAcceptedConnections == 2,
        "Two supported collinear gaps should be accepted by the final-object pass.");
    require(
        keptPolygonCount(result) == 1,
        "Three collinear segments should become one polygonal fault belt.");
    for (const fpt::WeakContinuityCandidate& candidate :
         result.weakContinuityCandidates) {
        if (candidate.stage == 3 && candidate.accepted) {
            require(
                candidate.mergedComponentHolesAfter == 0,
                "Accepted final-object bridges must remain free of internal holes.");
        }
    }
}

void testAmbiguousYJunctionDoesNotChooseAnArmArbitrarily() {
    const fpt::TraceResult result = fpt::traceFaultPolygons(
        makeAmbiguousJunctionFinalBridgeGrid(), finalObjectBridgeOptions());
    require(
        result.weakContinuityFinalObjectAcceptedConnections == 0,
        "A symmetric Y junction must not assign the continuation arbitrarily.");
    bool ambiguityReported = false;
    for (const fpt::WeakContinuityCandidate& candidate :
         result.weakContinuityCandidates) {
        ambiguityReported = ambiguityReported ||
            candidate.decision ==
                "final_bridge_ambiguous_junction_attribution" ||
            candidate.decision ==
                "final_bridge_unmatched_junction_branch";
    }
    require(
        ambiguityReported,
        "The rejected Y-junction bridge should report a branch-attribution reason.");
}

void testNearbyThirdTraceBlocksEnvelopeBridge() {
    const fpt::TraceResult result = fpt::traceFaultPolygons(
        makeNearbyThirdTraceFinalBridgeGrid(), finalObjectBridgeOptions());
    require(
        result.weakContinuityFinalObjectAcceptedConnections == 0,
        "A third trace inside the reconstructed belt clearance must block the bridge.");
    bool clearanceReported = false;
    for (const fpt::WeakContinuityCandidate& candidate :
         result.weakContinuityCandidates) {
        clearanceReported = clearanceReported ||
            candidate.decision ==
                "final_bridge_insufficient_third_object_clearance";
    }
    require(
        clearanceReported,
        "The nearby-third-trace rejection should be visible in diagnostics.");
}

} // namespace

int main() {
    try {
        testExperimentalGateReducesUnsupportedExtension();
        testLegalShortGapStillKeepsRemoteWeakExtension();
        testDetachedWeakEvidenceFallsBackToStrongResult();
        testWideWeakBlobIsRejectedByEndToEndEvidenceGate();
        testDefaultGateRemainsDisabled();
        testNonFiniteSupportRatioIsRejected();
        testCandidateRootGroupingRejectsUnsupportedCoreModes();
        testAcceptedWeakContinuitySurvivesFinalPolygonization();
        testFinalObjectPassConnectsThreeSupportedCollinearSegments();
        testAmbiguousYJunctionDoesNotChooseAnArmArbitrarily();
        testNearbyThirdTraceBlocksEnvelopeBridge();
        std::cout << "test_weak_path_support_integration: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_weak_path_support_integration: FAIL: "
                  << error.what() << "\n";
        return 1;
    }
}
