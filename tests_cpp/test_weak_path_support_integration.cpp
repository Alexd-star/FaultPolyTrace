#include "../src/core/FaultTracer.h"

#include <iostream>
#include <limits>
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
        std::cout << "test_weak_path_support_integration: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_weak_path_support_integration: FAIL: "
                  << error.what() << "\n";
        return 1;
    }
}
