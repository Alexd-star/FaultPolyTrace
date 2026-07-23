#include "../src/core/FaultTracer.h"
#include "../src/core/LocalPolygonQuality.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D makeGrid(int nx = 48, int ny = 28, double sx = 0.0, double sy = 0.0) {
    fpt::GridMetadata metadata;
    metadata.nx = nx;
    metadata.ny = ny;
    metadata.sx = sx;
    metadata.sy = sy;
    metadata.dx = 1.0;
    metadata.dy = 1.0;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 0.0;
    }
    return grid;
}

fpt::Point worldPoint(const fpt::Grid2D& grid, double x, double y) {
    return {grid.worldX(x), grid.worldY(y)};
}

fpt::Polygon makeBulgePolygon(const fpt::Grid2D& grid, int id = 7) {
    fpt::Polygon polygon;
    polygon.id = id;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 4, 10),
        worldPoint(grid, 38, 10),
        worldPoint(grid, 38, 12),
        worldPoint(grid, 24, 12),
        worldPoint(grid, 24, 17),
        worldPoint(grid, 22, 17),
        worldPoint(grid, 22, 12),
        worldPoint(grid, 4, 12)
    };
    polygon.area = fpt::polygonArea(polygon.points);
    polygon.perimeter = fpt::polygonPerimeter(polygon.points);
    return polygon;
}

void fillCoreSupport(fpt::Grid2D& grid, double value = 1.0) {
    for (int y = 10; y <= 11; ++y) {
        for (int x = 4; x <= 37; ++x) {
            grid.value(x, y) = value;
        }
    }
}

fpt::LocalPolygonQualityOptions permissiveOptions() {
    fpt::LocalPolygonQualityOptions options;
    options.widthRatio = 2.2;
    options.minExtraCells = 6;
    options.candidatePaddingCells = 1;
    options.supportThresholdRatio = 0.70;
    options.supportBufferCells = 1;
    options.minSupportCells = 4;
    options.maxAreaLossRatio = 0.25;
    options.maxStrongCoverageLossRatio = 0.0;
    options.maxWeakCoverageLossRatio = 0.0;
    options.maxBackgroundLeakageIncreaseRatio = 0.0;
    options.maxLocalizedSpanRatio = 0.45;
    options.endpointGuardRatio = 0.15;
    options.stabilityThresholdDeltaRatio = 0.02;
    options.minStabilityJaccard = 0.90;
    return options;
}

fpt::Grid2D makeTraceBulgeGrid() {
    fpt::Grid2D grid = makeGrid();
    for (int y = 10; y <= 11; ++y) {
        for (int x = 4; x <= 37; ++x) {
            grid.value(x, y) = 1.0;
        }
    }
    for (int y = 12; y <= 16; ++y) {
        for (int x = 22; x <= 23; ++x) {
            grid.value(x, y) = 0.15;
        }
    }
    return grid;
}

fpt::TraceOptions localQualityTraceOptions() {
    fpt::TraceOptions options;
    options.traceMode = fpt::TraceMode::Region;
    options.thresholdMode = fpt::ThresholdMode::Fixed;
    options.threshold = 0.10;
    options.minArea = 0.0;
    options.minCells = 1;
    options.openRadius = 0;
    options.closeRadius = 0;
    options.adaptiveAreaFilter = false;
    options.simplifyTolerance = 0.0;
    options.smoothIterations = 0;
    options.localPolygonQuality = true;
    options.localQualityWidthRatio = 2.2;
    options.localQualityMinExtraCells = 6;
    options.localQualitySupportThresholdRatio = 2.0;
    options.localQualitySupportBufferCells = 1;
    options.localQualityMaxAreaLossRatio = 0.50;
    options.localQualityMaxStrongCoverageLossRatio = 1.0;
    options.localQualityMaxWeakCoverageLossRatio = 1.0;
    options.localQualityMaxBackgroundLeakageIncreaseRatio = 1.0;
    options.localQualityStabilityThresholdDeltaRatio = 0.02;
    options.localQualityMinStabilityJaccard = 0.0;
    return options;
}

bool samePoints(const std::vector<fpt::Point>& left, const std::vector<fpt::Point>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::abs(left[i].x - right[i].x) > 1e-9 ||
            std::abs(left[i].y - right[i].y) > 1e-9) {
            return false;
        }
    }
    return true;
}

bool containsExactPoint(
    const std::vector<fpt::Point>& points,
    const fpt::Point& expected) {
    for (const fpt::Point& point : points) {
        if (point.x == expected.x && point.y == expected.y) {
            return true;
        }
    }
    return false;
}

const fpt::LocalPolygonQualityEvidence& onlyEvidence(
    const fpt::LocalPolygonQualityResult& result) {
    require(result.evidence.size() == 1, "Expected exactly one local-quality candidate.");
    return result.evidence.front();
}

const fpt::LocalPolygonQualityEvidence& evidenceOfType(
    const fpt::LocalPolygonQualityResult& result,
    fpt::LocalQualityCandidateType type) {
    const fpt::LocalPolygonQualityEvidence* found = nullptr;
    for (const auto& evidence : result.evidence) {
        if (evidence.type != type) {
            continue;
        }
        require(found == nullptr, "Expected one candidate of the requested type.");
        found = &evidence;
    }
    require(found != nullptr, "Expected candidate type was not detected.");
    return *found;
}

int countEvidenceType(
    const fpt::LocalPolygonQualityResult& result,
    fpt::LocalQualityCandidateType type) {
    int count = 0;
    for (const auto& evidence : result.evidence) {
        count += evidence.type == type ? 1 : 0;
    }
    return count;
}

void testLocalBulgeIsConservativelyTrimmed() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    const fpt::Polygon original = makeBulgePolygon(grid);

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {original}, 1.0, permissiveOptions());

    const auto& evidence = onlyEvidence(result);
    require(evidence.type == fpt::LocalQualityCandidateType::LocalBulge,
            "A localized unsupported widening must be classified as a bulge.");
    require(evidence.decision == fpt::LocalQualityDecision::Accepted,
            "A supported conservative bulge trim should pass all gates.");
    require(result.stats.acceptedCount == 1,
            "Accepted local repair must be counted.");
    require(result.polygons.front().area < original.area,
            "Accepted local bulge repair must reduce unsupported area.");
    require(result.polygons.front().id == original.id,
            "Local repair must preserve object identity.");
}

void testAcceptedRepairPreservesOriginalVerticesOutsideCandidateWindow() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    fpt::Polygon original = makeBulgePolygon(grid, 8);
    const fpt::Point firstOutside = worldPoint(grid, 8, 10);
    const fpt::Point secondOutside = worldPoint(grid, 30, 12);
    original.points.insert(original.points.begin() + 1, firstOutside);
    original.points.insert(original.points.begin() + 4, secondOutside);
    original.area = fpt::polygonArea(original.points);
    original.perimeter = fpt::polygonPerimeter(original.points);

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {original}, 1.0, permissiveOptions());
    const auto& evidence = evidenceOfType(
        result,
        fpt::LocalQualityCandidateType::LocalBulge);

    require(evidence.decision == fpt::LocalQualityDecision::Accepted,
            "The synthetic local bulge should pass the conservative gates.");
    require(containsExactPoint(result.polygons.front().points, firstOutside),
            "Accepted repair must preserve the first original outside-window vertex exactly.");
    require(containsExactPoint(result.polygons.front().points, secondOutside),
            "Accepted repair must preserve the second original outside-window vertex exactly.");
}

void testLowSupportBridgeIsReviewOnly() {
    fpt::Grid2D grid = makeGrid();
    fpt::Polygon polygon;
    polygon.id = 11;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 4, 10), worldPoint(grid, 38, 10),
        worldPoint(grid, 38, 13), worldPoint(grid, 4, 13)
    };
    polygon.area = fpt::polygonArea(polygon.points);
    for (int y = 10; y <= 12; ++y) {
        for (int x = 4; x <= 37; ++x) {
            grid.value(x, y) = (x >= 20 && x <= 22) ? 0.05 : 1.0;
        }
    }

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, permissiveOptions());
    const auto& evidence = onlyEvidence(result);

    require(evidence.type == fpt::LocalQualityCandidateType::LowSupportBridge,
            "A full-width low-support neck must be detected as a bridge.");
    require(evidence.decision == fpt::LocalQualityDecision::Review,
            "A bridge that could split one object must remain review-only.");
    require(result.stats.reviewCount == 1,
            "Review-only bridge evidence must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Review-only bridge evidence must preserve the original geometry.");
}

void testMixedBulgeAndBridgeCandidatesAreBothDetectedWithoutDuplicates() {
    fpt::Grid2D grid = makeGrid();
    fpt::Polygon polygon;
    polygon.id = 111;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 4, 10), worldPoint(grid, 44, 10),
        worldPoint(grid, 44, 13), worldPoint(grid, 16, 13),
        worldPoint(grid, 16, 18), worldPoint(grid, 13, 18),
        worldPoint(grid, 13, 13), worldPoint(grid, 4, 13)
    };
    polygon.area = fpt::polygonArea(polygon.points);
    for (int y = 10; y <= 12; ++y) {
        for (int x = 4; x <= 43; ++x) {
            grid.value(x, y) = (x >= 31 && x <= 33) ? 0.05 : 1.0;
        }
    }

    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.widthRatio = 2.2;
    options.maxAreaLossRatio = 0.40;
    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);

    require(countEvidenceType(result, fpt::LocalQualityCandidateType::LocalBulge) == 1,
            "A mixed object must retain one localized bulge candidate.");
    require(countEvidenceType(result, fpt::LocalQualityCandidateType::LowSupportBridge) == 1,
            "Width candidates must not suppress an independent low-support bridge.");
    require(result.evidence.size() == 2,
            "Mixed candidate detection must remove duplicate windows.");
}

void testCandidateAndEvidenceBudgetsStopExpensiveProcessing() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    std::vector<fpt::Polygon> polygons;
    for (int index = 0; index < 40; ++index) {
        polygons.push_back(makeBulgePolygon(grid, 3000 + index));
    }

    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.maxCandidatesPerObject = 1;
    options.maxTotalCandidates = 2;
    options.maxWindowCells = 10000;
    options.maxEvidenceRecords = 3;
    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, polygons, 1.0, options);

    require(result.stats.detectedCount >= 40,
            "Stress input should detect at least one candidate per object.");
    require(result.stats.candidateBudgetReviewCount >= 38,
            "Candidates above the global processing budget must become review evidence.");
    require(result.stats.totalEvidence ==
                static_cast<std::size_t>(result.stats.detectedCount),
            "Total evidence must count stored and truncated records.");
    require(result.stats.storedEvidence == 3,
            "Stored evidence must stop at the configured upper bound.");
    require(result.stats.truncatedEvidence == result.stats.totalEvidence - 3,
            "Truncated evidence count must remain auditable.");
    require(result.evidence.size() == 3,
            "The evidence vector must not exceed its storage budget.");
}

void testWindowBudgetReturnsReviewWithoutRepair() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    const fpt::Polygon polygon = makeBulgePolygon(grid, 3041);
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.maxWindowCells = 1;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);

    require(result.stats.windowBudgetReviewCount >= 1,
            "An oversized candidate window must be counted as budget review.");
    require(result.stats.acceptedCount == 0,
            "An oversized candidate window must not enter expensive repair gates.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Window-budget review must preserve the original geometry.");
}

void testObjectCellBudgetStopsCandidateEvaluation() {
    fpt::Grid2D grid = makeGrid(1501, 701);
    fpt::Polygon polygon;
    polygon.id = 3044;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 0, 0), worldPoint(grid, 1500, 0),
        worldPoint(grid, 1500, 700), worldPoint(grid, 0, 700)
    };
    polygon.holes = {{
        worldPoint(grid, 2, 2), worldPoint(grid, 2, 698),
        worldPoint(grid, 1498, 698), worldPoint(grid, 1498, 2)
    }};
    polygon.area = fpt::polygonArea(polygon.points);
    polygon.perimeter = fpt::polygonPerimeter(polygon.points);

    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.maxObjectCells = 10000;
    options.maxWindowCells = 1;
    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);

    require(result.stats.objectBudgetReviewCount == 1,
            "An oversized object must produce visible object-budget review statistics.");
    require(result.stats.detectedCount == 1 && result.stats.reviewCount == 1,
            "An oversized object must produce exactly one review evidence record.");
    require(result.stats.acceptedCount == 0 && result.stats.rejectedCount == 0,
            "An oversized object must not enter candidate proposal evaluation.");
    require(result.stats.windowBudgetReviewCount == 0 &&
                result.stats.candidateBudgetReviewCount == 0 &&
                result.stats.topologyRejectedCount == 0 &&
                result.stats.stabilityRejectedCount == 0,
            "Object-budget gating must run before candidate and expensive quality gates.");
    require(result.evidence.size() == 1 &&
                result.evidence.front().type ==
                    fpt::LocalQualityCandidateType::ObjectBudgetExceeded,
            "Object-budget review evidence must be explicit and auditable.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Object-budget review must preserve the original geometry.");
}

void testZeroEvidenceBudgetStoresNothingButCountsTruncation() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.maxEvidenceRecords = 0;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {makeBulgePolygon(grid, 3042)}, 1.0, options);
    require(result.stats.totalEvidence >= 1,
            "Evidence totals must still be counted when storage is disabled.");
    require(result.stats.storedEvidence == 0 && result.evidence.empty(),
            "A zero evidence budget must retain no candidate array entries.");
    require(result.stats.truncatedEvidence == result.stats.totalEvidence,
            "Every unstored record must be counted as truncated.");
}

void testUnsafeBudgetParametersAreRejected() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    const fpt::Polygon polygon = makeBulgePolygon(grid, 3043);
    const auto requireRejected = [&](fpt::LocalPolygonQualityOptions options) {
        bool rejected = false;
        try {
            (void)fpt::applyLocalPolygonQuality(grid, {polygon}, 1.0, options);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "Zero processing or window budgets must be rejected.");
    };

    fpt::LocalPolygonQualityOptions perObject = permissiveOptions();
    perObject.maxCandidatesPerObject = 0;
    requireRejected(perObject);
    fpt::LocalPolygonQualityOptions global = permissiveOptions();
    global.maxTotalCandidates = 0;
    requireRejected(global);
    fpt::LocalPolygonQualityOptions window = permissiveOptions();
    window.maxWindowCells = 0;
    requireRejected(window);
    fpt::LocalPolygonQualityOptions object = permissiveOptions();
    object.maxObjectCells = 0;
    requireRejected(object);
}

void testNonFiniteAndExtremeRingCoordinatesAreSafelyRejected() {
    fpt::Grid2D grid = makeGrid();
    fpt::Polygon nonFinite = makeBulgePolygon(grid, 3050);
    nonFinite.points[2].x = std::numeric_limits<double>::quiet_NaN();
    fpt::Polygon extreme = makeBulgePolygon(grid, 3051);
    extreme.points[2].x = 1e300;

    for (const fpt::Polygon& polygon : {nonFinite, extreme}) {
        const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
            grid, {polygon}, 1.0, permissiveOptions());
        const auto& evidence = onlyEvidence(result);
        require(evidence.decision == fpt::LocalQualityDecision::Rejected,
                "Unsafe ring coordinates must be rejected without allocation failure.");
        require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::InvalidGeometry,
                "Unsafe ring coordinates need a dedicated rejection reason.");
        require(result.stats.invalidGeometryRejectedCount == 1,
                "Invalid geometry rejection must be counted.");
        require(result.polygons.front().points.size() == polygon.points.size(),
                "Invalid geometry rejection must retain the original ring container.");
    }
}

void testLegalWideBeltIsNotTrimmed() {
    fpt::Grid2D grid = makeGrid();
    fpt::Polygon polygon;
    polygon.id = 12;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 4, 9), worldPoint(grid, 38, 9),
        worldPoint(grid, 38, 16), worldPoint(grid, 4, 16)
    };
    polygon.area = fpt::polygonArea(polygon.points);
    for (int y = 9; y <= 15; ++y) {
        for (int x = 4; x <= 37; ++x) {
            grid.value(x, y) = 1.0;
        }
    }

    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.widthRatio = 1.4;
    options.referenceWidthCells = 2.0;
    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = onlyEvidence(result);

    require(evidence.type == fpt::LocalQualityCandidateType::LegalWideBelt,
            "A long attribute-supported wide belt must be classified separately.");
    require(evidence.decision == fpt::LocalQualityDecision::Review,
            "A legal wide belt must not be automatically narrowed.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Legal wide-belt review must preserve geometry.");
}

void testNaturalEndCapIsNotTrimmed() {
    fpt::Grid2D grid = makeGrid();
    fpt::Polygon polygon;
    polygon.id = 13;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 4, 10), worldPoint(grid, 38, 10),
        worldPoint(grid, 38, 16), worldPoint(grid, 34, 16),
        worldPoint(grid, 34, 12), worldPoint(grid, 4, 12)
    };
    polygon.area = fpt::polygonArea(polygon.points);
    for (int y = 10; y <= 11; ++y) {
        for (int x = 4; x <= 37; ++x) {
            grid.value(x, y) = 1.0;
        }
    }
    for (int y = 12; y <= 15; ++y) {
        for (int x = 34; x <= 37; ++x) {
            grid.value(x, y) = 0.9;
        }
    }

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, permissiveOptions());
    const auto& evidence = onlyEvidence(result);

    require(evidence.type == fpt::LocalQualityCandidateType::NaturalEndCap,
            "Endpoint widening must be classified as a natural cap candidate.");
    require(evidence.decision == fpt::LocalQualityDecision::Review,
            "Natural endpoint caps must remain unchanged for review.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Natural endpoint cap review must preserve geometry.");
}

void testInsufficientEvidenceFallsBack() {
    fpt::Grid2D grid = makeGrid();
    const fpt::Polygon polygon = makeBulgePolygon(grid, 21);
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.minSupportCells = 20;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = evidenceOfType(
        result,
        fpt::LocalQualityCandidateType::LocalBulge);

    require(evidence.decision == fpt::LocalQualityDecision::Rejected,
            "A candidate without attribute evidence must be rejected.");
    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::InsufficientEvidence,
            "Evidence failure must have a specific rejection reason.");
    require(result.stats.insufficientEvidenceRejectedCount == 1,
            "Insufficient-evidence rejection must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Evidence failure must return the original geometry.");
}

void testExcessiveAreaLossFallsBack() {
    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    const fpt::Polygon polygon = makeBulgePolygon(grid, 22);
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.maxAreaLossRatio = 0.01;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = onlyEvidence(result);

    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::ExcessiveAreaLoss,
            "Excessive area loss must reject the repair explicitly.");
    require(result.stats.areaLossRejectedCount == 1,
            "Area-loss rejection must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Area-loss failure must return the original geometry.");
}

void testStrongCoverageGateRejectsAndCounts() {
    fpt::Grid2D grid = makeGrid();
    const fpt::Polygon polygon = makeBulgePolygon(grid, 221);
    fillCoreSupport(grid, 1.20);
    for (int y = 12; y <= 16; ++y) {
        for (int x = 22; x <= 23; ++x) {
            grid.value(x, y) = 1.0;
        }
    }
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.supportThresholdRatio = 1.10;
    options.maxAreaLossRatio = 0.50;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = onlyEvidence(result);

    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::StrongCoverageLoss,
            "Removing strong cells must reach the strong-coverage rejection gate.");
    require(result.stats.strongCoverageRejectedCount == 1,
            "Strong-coverage rejection must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Strong-coverage rejection must return the original geometry.");
}

void testWeakCoverageGateRejectsWithoutStrongCoverageFailure() {
    fpt::Grid2D grid = makeGrid();
    const fpt::Polygon polygon = makeBulgePolygon(grid, 224);
    fillCoreSupport(grid, 1.20);
    for (int y = 12; y <= 16; ++y) {
        for (int x = 22; x <= 23; ++x) {
            grid.value(x, y) = 0.80;
        }
    }
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.supportThresholdRatio = 0.90;
    options.maxAreaLossRatio = 0.50;
    options.maxStrongCoverageLossRatio = 0.0;
    options.maxWeakCoverageLossRatio = 0.0;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = onlyEvidence(result);

    require(evidence.strongCoverageLossRatio == 0.0,
            "Weak-coverage test must not lose strong response cells.");
    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::WeakCoverageLoss,
            "Weak response loss must reach the weak-coverage rejection gate.");
    require(result.stats.strongCoverageRejectedCount == 0,
            "Weak-coverage rejection must not be counted as strong-coverage loss.");
    require(result.stats.weakCoverageRejectedCount == 1,
            "Weak-coverage rejection must be counted independently.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Weak-coverage rejection must return the original geometry.");
}

void testBackgroundLeakageGateRejectsAndCounts() {
    fpt::Grid2D grid = makeGrid();
    const fpt::Polygon polygon = makeBulgePolygon(grid, 222);
    fillCoreSupport(grid, 1.20);
    for (int x = 22; x <= 23; ++x) {
        grid.value(x, 12) = 0.0;
        for (int y = 13; y <= 16; ++y) {
            grid.value(x, y) = 0.80;
        }
    }
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.supportThresholdRatio = 1.10;
    options.maxAreaLossRatio = 0.80;
    options.maxStrongCoverageLossRatio = 1.0;
    options.maxWeakCoverageLossRatio = 1.0;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = onlyEvidence(result);

    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::BackgroundLeakage,
            std::string("A higher background fraction must reach the leakage rejection gate; got ") +
                fpt::localQualityRejectionReasonName(evidence.rejectionReason));
    require(result.stats.backgroundLeakageRejectedCount == 1,
            "Background-leakage rejection must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Background-leakage rejection must return the original geometry.");
}

void testStabilityGateRejectsAndCounts() {
    fpt::Grid2D grid = makeGrid();
    const fpt::Polygon polygon = makeBulgePolygon(grid, 223);
    fillCoreSupport(grid);
    for (int x = 22; x <= 23; ++x) {
        for (int y = 12; y <= 13; ++y) {
            grid.value(x, y) = 0.70;
        }
        for (int y = 14; y <= 16; ++y) {
            grid.value(x, y) = 0.05;
        }
    }
    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.maxAreaLossRatio = 0.50;
    options.minStabilityJaccard = 0.999;

    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = onlyEvidence(result);

    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::StabilityRisk,
            "Threshold-sensitive support must reach the stability rejection gate.");
    require(result.stats.stabilityRejectedCount == 1,
            "Stability rejection must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Stability rejection must return the original geometry.");
}

void testTopologyRiskFallsBack() {
    fpt::Grid2D grid = makeGrid();
    fpt::Polygon polygon;
    polygon.id = 23;
    polygon.kept = true;
    polygon.points = {
        worldPoint(grid, 4, 10), worldPoint(grid, 18, 10),
        worldPoint(grid, 18, 7), worldPoint(grid, 24, 7),
        worldPoint(grid, 24, 10), worldPoint(grid, 38, 10),
        worldPoint(grid, 38, 12), worldPoint(grid, 24, 12),
        worldPoint(grid, 24, 15), worldPoint(grid, 18, 15),
        worldPoint(grid, 18, 12), worldPoint(grid, 4, 12)
    };
    polygon.area = fpt::polygonArea(polygon.points);
    for (int y = 10; y <= 11; ++y) {
        for (int x = 4; x <= 17; ++x) {
            grid.value(x, y) = 1.0;
        }
        for (int x = 24; x <= 37; ++x) {
            grid.value(x, y) = 1.0;
        }
    }
    for (int y = 7; y <= 14; ++y) {
        for (int x = 18; x <= 23; ++x) {
            grid.value(x, y) = 0.05;
        }
    }

    fpt::LocalPolygonQualityOptions options = permissiveOptions();
    options.widthRatio = 2.0;
    options.referenceWidthCells = 2.0;
    options.maxLocalizedSpanRatio = 0.35;
    options.maxAreaLossRatio = 0.90;
    const fpt::LocalPolygonQualityResult result = fpt::applyLocalPolygonQuality(
        grid, {polygon}, 1.0, options);
    const auto& evidence = evidenceOfType(
        result,
        fpt::LocalQualityCandidateType::LocalBulge);

    require(evidence.decision == fpt::LocalQualityDecision::Rejected,
            "A repair that splits or invalidates one object must be rejected.");
    require(evidence.rejectionReason == fpt::LocalQualityRejectionReason::TopologyRisk,
            "Topology failure must have a specific rejection reason.");
    require(result.stats.topologyRejectedCount == 1,
            "Topology rejection must be counted.");
    require(samePoints(result.polygons.front().points, polygon.points),
            "Topology failure must return the original geometry.");
}

void testCoordinateAndObjectIdDoNotAffectDecision() {
    fpt::Grid2D originGrid = makeGrid();
    fillCoreSupport(originGrid);
    const fpt::Polygon originPolygon = makeBulgePolygon(originGrid, 31);
    const fpt::LocalPolygonQualityResult origin = fpt::applyLocalPolygonQuality(
        originGrid, {originPolygon}, 1.0, permissiveOptions());

    fpt::Grid2D shiftedGrid = makeGrid(48, 28, 10000.0, -7000.0);
    fillCoreSupport(shiftedGrid);
    const fpt::Polygon shiftedPolygon = makeBulgePolygon(shiftedGrid, 9031);
    const fpt::LocalPolygonQualityResult shifted = fpt::applyLocalPolygonQuality(
        shiftedGrid, {shiftedPolygon}, 1.0, permissiveOptions());

    const auto& originEvidence = onlyEvidence(origin);
    const auto& shiftedEvidence = onlyEvidence(shifted);
    require(originEvidence.type == shiftedEvidence.type,
            "Candidate classification must not depend on world coordinates or object id.");
    require(originEvidence.decision == shiftedEvidence.decision,
            "Candidate decision must not depend on world coordinates or object id.");
    require(std::abs(origin.polygons.front().area - shifted.polygons.front().area) < 1e-9,
            "Translated geometry must produce the same repaired area.");
}

void testDefaultsAndUnsafeParameters() {
    const fpt::TraceOptions traceOptions;
    require(!traceOptions.localPolygonQuality,
            "Experimental local polygon quality must remain disabled by default.");

    fpt::Grid2D grid = makeGrid();
    fillCoreSupport(grid);
    const fpt::Polygon polygon = makeBulgePolygon(grid);
    const auto requireRejected = [&](fpt::LocalPolygonQualityOptions options) {
        bool rejected = false;
        try {
            (void)fpt::applyLocalPolygonQuality(grid, {polygon}, 1.0, options);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        require(rejected, "Unsafe local-quality options must be rejected.");
    };

    fpt::LocalPolygonQualityOptions nan = permissiveOptions();
    nan.widthRatio = std::numeric_limits<double>::quiet_NaN();
    requireRejected(nan);

    fpt::LocalPolygonQualityOptions negative = permissiveOptions();
    negative.supportBufferCells = -1;
    requireRejected(negative);

    fpt::LocalPolygonQualityOptions outOfRange = permissiveOptions();
    outOfRange.maxAreaLossRatio = 1.1;
    requireRejected(outOfRange);
}

void testFaultTracerDoesNotValidateDisabledLocalQualityOptions() {
    const fpt::Grid2D grid = makeTraceBulgeGrid();
    fpt::TraceOptions baselineOptions = localQualityTraceOptions();
    baselineOptions.localPolygonQuality = false;
    const fpt::TraceResult baseline = fpt::traceFaultPolygons(
        grid,
        baselineOptions);

    fpt::TraceOptions disabledOptions = baselineOptions;
    disabledOptions.localQualityWidthRatio =
        std::numeric_limits<double>::quiet_NaN();
    disabledOptions.localQualityMaxCandidatesPerObject = 0;
    disabledOptions.localQualityMaxTotalCandidates = 0;
    disabledOptions.localQualityMaxWindowCells = 0;
    const fpt::TraceResult disabled = fpt::traceFaultPolygons(
        grid,
        disabledOptions);

    require(disabled.activeCellCount == baseline.activeCellCount,
            "Disabled local quality must not change segmentation-mask statistics.");
    require(disabled.polygons.size() == baseline.polygons.size(),
            "Disabled local quality must remain isolated from polygon output.");
    require(disabled.localPolygonQualityStats.totalEvidence == 0,
            "Disabled local quality must not emit candidate evidence.");
}

void testFaultTracerFiltersAcceptedRepairByFinalAreaAndCells() {
    const fpt::Grid2D grid = makeTraceBulgeGrid();
    fpt::TraceOptions originalOptions = localQualityTraceOptions();
    originalOptions.localPolygonQuality = false;
    const fpt::TraceResult original = fpt::traceFaultPolygons(
        grid,
        originalOptions);

    const fpt::TraceOptions repairedOptions = localQualityTraceOptions();
    const fpt::TraceResult repaired = fpt::traceFaultPolygons(
        grid,
        repairedOptions);
    require(original.polygons.size() == 1 && repaired.polygons.size() == 1,
            "The integration fixture must produce one polygon.");
    require(repaired.localPolygonQualityStats.acceptedCount >= 1,
            "The integration fixture must exercise an accepted repair.");
    require(repaired.polygons.front().area < original.polygons.front().area,
            "The accepted repair must reduce area before final filtering.");
    require(repaired.polygons.front().cellCount < original.polygons.front().cellCount,
            "The accepted repair must reduce rasterized cell count.");
    require(repaired.activeCellCount == original.activeCellCount,
            "activeCellCount must retain segmentation-mask semantics after repair.");

    fpt::TraceOptions areaOptions = repairedOptions;
    areaOptions.minArea = 0.5 * (
        original.polygons.front().area + repaired.polygons.front().area);
    const fpt::TraceResult areaFiltered = fpt::traceFaultPolygons(
        grid,
        areaOptions);
    require(!areaFiltered.polygons.front().kept,
            "A repair falling below minArea must update polygon status to filtered.");
    require(areaFiltered.localPolygonQualityStats.postRepairFilteredCount == 1,
            "Post-repair area filtering must be counted once.");

    fpt::TraceOptions cellOptions = repairedOptions;
    cellOptions.minCells = repaired.polygons.front().cellCount + 1;
    require(cellOptions.minCells <= original.polygons.front().cellCount,
            "The fixture must leave room for a post-repair minCells threshold.");
    const fpt::TraceResult cellFiltered = fpt::traceFaultPolygons(
        grid,
        cellOptions);
    require(!cellFiltered.polygons.front().kept,
            "A repair falling below minCells must update polygon status to filtered.");
    require(cellFiltered.localPolygonQualityStats.postRepairFilteredCount == 1,
            "Post-repair cell filtering must be counted once.");
}

void testFaultTracerLocalQualityStatisticsStayConsistent() {
    const fpt::Grid2D grid = makeTraceBulgeGrid();
    fpt::TraceOptions options = localQualityTraceOptions();
    options.localQualityMaxEvidenceRecords = 0;
    const fpt::TraceResult result = fpt::traceFaultPolygons(grid, options);
    const fpt::LocalPolygonQualityStats& stats =
        result.localPolygonQualityStats;

    require(stats.detectedCount ==
                stats.acceptedCount + stats.reviewCount + stats.rejectedCount,
            "Every detected candidate must have exactly one final decision.");
    require(stats.totalEvidence == static_cast<std::size_t>(stats.detectedCount),
            "Evidence totals must match candidate decisions.");
    require(stats.storedEvidence == result.localPolygonQualityEvidence.size(),
            "Stored evidence statistics must match the returned array.");
    require(stats.storedEvidence == 0 && result.localPolygonQualityEvidence.empty(),
            "A zero evidence budget must store no candidate records.");
    require(stats.truncatedEvidence == stats.totalEvidence,
            "A zero evidence budget must count every record as truncated.");
}

} // namespace

int main() {
    try {
        testLocalBulgeIsConservativelyTrimmed();
        testAcceptedRepairPreservesOriginalVerticesOutsideCandidateWindow();
        testLowSupportBridgeIsReviewOnly();
        testMixedBulgeAndBridgeCandidatesAreBothDetectedWithoutDuplicates();
        testCandidateAndEvidenceBudgetsStopExpensiveProcessing();
        testWindowBudgetReturnsReviewWithoutRepair();
        testObjectCellBudgetStopsCandidateEvaluation();
        testZeroEvidenceBudgetStoresNothingButCountsTruncation();
        testUnsafeBudgetParametersAreRejected();
        testNonFiniteAndExtremeRingCoordinatesAreSafelyRejected();
        testLegalWideBeltIsNotTrimmed();
        testNaturalEndCapIsNotTrimmed();
        testInsufficientEvidenceFallsBack();
        testExcessiveAreaLossFallsBack();
        testStrongCoverageGateRejectsAndCounts();
        testWeakCoverageGateRejectsWithoutStrongCoverageFailure();
        testBackgroundLeakageGateRejectsAndCounts();
        testStabilityGateRejectsAndCounts();
        testTopologyRiskFallsBack();
        testCoordinateAndObjectIdDoNotAffectDecision();
        testDefaultsAndUnsafeParameters();
        testFaultTracerDoesNotValidateDisabledLocalQualityOptions();
        testFaultTracerFiltersAcceptedRepairByFinalAreaAndCells();
        testFaultTracerLocalQualityStatisticsStayConsistent();
        std::cout << "test_local_polygon_quality: PASS\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_local_polygon_quality: FAIL: " << error.what() << "\n";
        return 1;
    }
}
