#include "../src/core/MaskTopology.h"
#include "../src/core/WeakTailTopologyGate.h"

#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

fpt::WeakTailResult makeTailResult(
    const fpt::BinaryMask& base,
    const std::vector<fpt::SkeletonPoint>& cells) {
    fpt::WeakTailResult result{
        fpt::BinaryMask(base),
        fpt::BinaryMask(base.width(), base.height()),
        {},
        {}};
    fpt::WeakTailCandidate candidate;
    candidate.id = 1;
    candidate.accepted = true;
    candidate.decision = "accepted";
    candidate.cells = cells;
    candidate.cellCount = static_cast<int>(cells.size());
    for (const fpt::SkeletonPoint& point : cells) {
        result.mask.set(point.x, point.y, true);
        result.acceptedMask.set(point.x, point.y, true);
    }
    result.stats.acceptedComponents = 1;
    result.stats.acceptedCells = candidate.cellCount;
    result.candidates.push_back(candidate);
    return result;
}

void testLoopClosingTailIsMinimallyTrimmed() {
    constexpr int width = 24;
    constexpr int height = 18;
    fpt::BinaryMask base(width, height);
    for (int y = 4; y <= 13; ++y) {
        base.set(5, y, true);
        base.set(18, y, true);
    }
    for (int x = 5; x <= 18; ++x) {
        base.set(x, 13, true);
    }
    for (int x = 5; x <= 10; ++x) {
        base.set(x, 4, true);
    }
    for (int x = 13; x <= 18; ++x) {
        base.set(x, 4, true);
    }
    const std::vector<fpt::SkeletonPoint> closingTail = {
        {11, 4}, {12, 4}};
    fpt::WeakTailResult tails = makeTailResult(base, closingTail);
    fpt::BinaryMask observed = fpt::unionMask(base, tails.acceptedMask);
    fpt::BinaryMask attribute(width, height);
    fpt::BinaryMask continuity(width, height);
    fpt::WeakTailTopologyGateOptions options;
    fpt::applyWeakTailTopologyGate(
        base, observed, attribute, continuity, options, tails);

    require(tails.stats.topologyInitialNewEnclosures == 1,
            "The gate must detect the new enclosed inactive region.");
    require(tails.stats.topologyRejectedComponents == 0 &&
                tails.stats.topologyTrimmedComponents == 1 &&
                tails.stats.topologyTrimmedCells == 1,
            "Only the minimum free-tip cell needed to open the loop should be trimmed.");
    require(tails.stats.topologyRemainingNewEnclosures == 0,
            "No new enclosure may remain after the gate.");
    require(tails.candidates.front().accepted &&
                tails.candidates.front().decision ==
                    "accepted_topology_trimmed" &&
                tails.mask.get(11, 4) && !tails.mask.get(12, 4),
            "The owner-attached prefix must remain while the closing free tip is removed.");
}

void testOpenTailExtensionIsAccepted() {
    constexpr int width = 24;
    constexpr int height = 18;
    fpt::BinaryMask base(width, height);
    for (int x = 3; x <= 10; ++x) {
        base.set(x, 8, true);
    }
    const std::vector<fpt::SkeletonPoint> extension = {
        {11, 8}, {12, 8}, {13, 8}, {14, 8}};
    fpt::WeakTailResult tails = makeTailResult(base, extension);
    fpt::BinaryMask observed = fpt::unionMask(base, tails.acceptedMask);
    fpt::BinaryMask attribute(width, height);
    fpt::BinaryMask continuity(width, height);
    fpt::WeakTailTopologyGateOptions options;
    fpt::applyWeakTailTopologyGate(
        base, observed, attribute, continuity, options, tails);

    require(tails.stats.topologyInitialNewEnclosures == 0,
            "An open extension must not be diagnosed as a new hole.");
    require(tails.stats.topologyRejectedComponents == 0,
            "A topology-safe tail extension must remain accepted.");
    require(tails.candidates.front().accepted && tails.mask.get(14, 8),
            "The accepted extension must remain in the output mask.");
}

void testExistingHoleDoesNotCauseFalseRejection() {
    constexpr int width = 24;
    constexpr int height = 18;
    fpt::BinaryMask base(width, height);
    for (int x = 4; x <= 16; ++x) {
        base.set(x, 4, true);
        base.set(x, 12, true);
    }
    for (int y = 4; y <= 12; ++y) {
        base.set(4, y, true);
        base.set(16, y, true);
    }
    const std::vector<fpt::SkeletonPoint> extension = {
        {17, 12}, {18, 12}, {19, 12}};
    fpt::WeakTailResult tails = makeTailResult(base, extension);
    fpt::BinaryMask observed = fpt::unionMask(base, tails.acceptedMask);
    fpt::BinaryMask attribute(width, height);
    fpt::BinaryMask continuity(width, height);
    fpt::WeakTailTopologyGateOptions options;
    fpt::applyWeakTailTopologyGate(
        base, observed, attribute, continuity, options, tails);

    require(fpt::countInactiveHoles(base) == 1,
            "The fixture must contain one pre-existing hole.");
    require(tails.stats.topologyInitialNewEnclosures == 0 &&
                tails.candidates.front().accepted,
            "The gate must compare against the base topology, not ban old holes.");
}

} // namespace

int main() {
    try {
        testLoopClosingTailIsMinimallyTrimmed();
        testOpenTailExtensionIsAccepted();
        testExistingHoleDoesNotCauseFalseRejection();
        std::cout << "Weak-tail topology gate tests passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
