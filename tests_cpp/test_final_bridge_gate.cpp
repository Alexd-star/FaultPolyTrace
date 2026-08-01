#include "../src/core/FinalBridgeGate.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::Grid2D supportedGrid(int width = 32, int height = 24) {
    fpt::GridMetadata metadata;
    metadata.nx = width;
    metadata.ny = height;
    fpt::Grid2D grid(metadata);
    for (double& value : grid.values()) {
        value = 1.0;
    }
    return grid;
}

void fillRectangle(
    fpt::BinaryMask& mask,
    int minX,
    int minY,
    int maxX,
    int maxY) {
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            mask.set(x, y, true);
        }
    }
}

fpt::WeakContinuityCandidate bridgeCandidate(
    int id,
    int startX,
    int endX,
    int y,
    double cost = 1.0) {
    fpt::WeakContinuityCandidate candidate;
    candidate.id = id;
    candidate.stage = 3;
    candidate.accepted = true;
    candidate.normalizedCost = cost;
    candidate.supportRatio = 1.0;
    candidate.meanSupport = 1.0;
    candidate.widthA = 1.0;
    candidate.widthB = 1.0;
    candidate.minimumSupportedValue = 0.5;
    candidate.endpointA = {startX, y};
    candidate.endpointB = {endX, y};
    candidate.pathLength = static_cast<double>(endX - startX);
    for (int x = startX; x <= endX; ++x) {
        candidate.path.push_back({x, y});
    }
    candidate.addedPathPixels =
        static_cast<int>(candidate.path.size()) - 2;
    return candidate;
}

fpt::FinalBridgeGateOptions gateOptions() {
    fpt::FinalBridgeGateOptions options;
    options.supportThreshold = 0.5;
    options.junctionInfluenceDistance = 4;
    options.maximumBeltRadius = 1;
    options.supportEnvelopeRadius = 0;
    return options;
}

void testAcceptsSupportedTwoObjectBridge() {
    fpt::Grid2D grid = supportedGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillRectangle(active, 4, 9, 7, 11);
    fillRectangle(active, 13, 9, 16, 11);

    fpt::WeakContinuityResult pass;
    pass.candidates.push_back(bridgeCandidate(1, 7, 13, 10));
    const fpt::FinalBridgeGateResult result =
        fpt::applyFinalObjectBridgeGates(
            grid, active, {}, pass, gateOptions());

    require(result.acceptedConnections == 1,
            "A supported bridge between exactly two objects must be accepted.");
    require(result.rejectedConnections == 0,
            "The accepted two-object bridge must not be counted as rejected.");
    require(pass.candidates.front().accepted,
            "The accepted candidate flag must be preserved in the evidence record.");
    require(result.mask.get(10, 10),
            "The accepted bridge envelope must be committed to the output mask.");
}

void testRejectsThirdObjectClearanceConflict() {
    fpt::Grid2D grid = supportedGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillRectangle(active, 4, 9, 7, 11);
    fillRectangle(active, 13, 9, 16, 11);
    active.set(10, 8, true);

    fpt::WeakContinuityResult pass;
    pass.candidates.push_back(bridgeCandidate(1, 7, 13, 10));
    const fpt::FinalBridgeGateResult result =
        fpt::applyFinalObjectBridgeGates(
            grid, active, {}, pass, gateOptions());

    require(result.acceptedConnections == 0 && result.rejectedConnections == 1,
            "A bridge violating third-object clearance must be rejected.");
    require(pass.candidates.front().decision ==
                "final_bridge_insufficient_third_object_clearance",
            "Third-object rejection must retain a specific diagnostic reason.");
}

void testRejectsEndpointComponentWithHole() {
    fpt::Grid2D grid = supportedGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillRectangle(active, 3, 7, 7, 7);
    fillRectangle(active, 3, 11, 7, 11);
    fillRectangle(active, 3, 8, 3, 10);
    fillRectangle(active, 7, 8, 7, 10);
    fillRectangle(active, 13, 8, 16, 10);

    fpt::WeakContinuityResult pass;
    pass.candidates.push_back(bridgeCandidate(1, 7, 13, 9));
    const fpt::FinalBridgeGateResult result =
        fpt::applyFinalObjectBridgeGates(
            grid, active, {}, pass, gateOptions());

    require(result.acceptedConnections == 0 && result.rejectedConnections == 1,
            "A bridge attached to an object with an internal hole must be rejected.");
    require(pass.candidates.front().endpointComponentHolesA == 1,
            "The endpoint component hole count must be computed exactly.");
}

void testIncrementalOwnershipRejectsRedundantBridge() {
    fpt::Grid2D grid = supportedGrid();
    fpt::BinaryMask active(grid.nx(), grid.ny());
    fillRectangle(active, 4, 9, 7, 11);
    fillRectangle(active, 13, 9, 16, 11);

    fpt::WeakContinuityResult pass;
    pass.candidates.push_back(bridgeCandidate(1, 7, 13, 10, 0.5));
    pass.candidates.push_back(bridgeCandidate(2, 7, 13, 10, 0.8));
    const fpt::FinalBridgeGateResult result =
        fpt::applyFinalObjectBridgeGates(
            grid, active, {}, pass, gateOptions());

    require(result.acceptedConnections == 1 && result.rejectedConnections == 1,
            "Only the first of two redundant bridges may be committed.");
    require(pass.candidates[1].decision ==
                "final_bridge_already_connected_by_prior_bridge",
            "Incremental ownership must identify objects merged by a prior bridge.");
}

} // namespace

int main() {
    try {
        testAcceptsSupportedTwoObjectBridge();
        testRejectsThirdObjectClearanceConflict();
        testRejectsEndpointComponentWithHole();
        testIncrementalOwnershipRejectsRedundantBridge();
        std::cout << "Final bridge gate tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "test_final_bridge_gate: FAIL: " << error.what() << '\n';
        return 1;
    }
}
