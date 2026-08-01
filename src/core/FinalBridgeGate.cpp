#include "FinalBridgeGate.h"

#include "BranchAttribution.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace fpt {
namespace {

using GateClock = std::chrono::steady_clock;

double elapsedMilliseconds(GateClock::time_point start) {
    return std::chrono::duration<double, std::milli>(
        GateClock::now() - start).count();
}

struct Cell {
    int x = 0;
    int y = 0;
};

struct CellBounds {
    int minX = 0;
    int minY = 0;
    int maxX = -1;
    int maxY = -1;

    bool valid() const {
        return minX <= maxX && minY <= maxY;
    }

    void include(int x, int y) {
        if (!valid()) {
            minX = maxX = x;
            minY = maxY = y;
            return;
        }
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }

    void include(const CellBounds& other) {
        if (!other.valid()) {
            return;
        }
        include(other.minX, other.minY);
        include(other.maxX, other.maxY);
    }
};

template <typename ActivePredicate>
int countInactiveHoles(
    int maskWidth,
    int maskHeight,
    CellBounds bounds,
    ActivePredicate&& isActive) {
    bounds.minX = std::max(0, bounds.minX);
    bounds.minY = std::max(0, bounds.minY);
    bounds.maxX = std::min(maskWidth - 1, bounds.maxX);
    bounds.maxY = std::min(maskHeight - 1, bounds.maxY);
    if (!bounds.valid()) {
        return 0;
    }

    const int width = bounds.maxX - bounds.minX + 1;
    const int height = bounds.maxY - bounds.minY + 1;
    std::vector<unsigned char> visited(
        static_cast<std::size_t>(width * height), 0);
    constexpr int offsets[4][2] = {
        {-1, 0}, {1, 0}, {0, -1}, {0, 1}
    };
    auto localIndex = [&bounds, width](int x, int y) {
        return (y - bounds.minY) * width + (x - bounds.minX);
    };

    int holes = 0;
    for (int y0 = bounds.minY; y0 <= bounds.maxY; ++y0) {
        for (int x0 = bounds.minX; x0 <= bounds.maxX; ++x0) {
            const std::size_t start =
                static_cast<std::size_t>(localIndex(x0, y0));
            if (isActive(x0, y0) || visited[start]) {
                continue;
            }
            bool touchesWindowBorder = false;
            std::queue<Cell> queue;
            queue.push({x0, y0});
            visited[start] = 1;
            while (!queue.empty()) {
                const Cell cell = queue.front();
                queue.pop();
                touchesWindowBorder = touchesWindowBorder ||
                    cell.x == bounds.minX || cell.x == bounds.maxX ||
                    cell.y == bounds.minY || cell.y == bounds.maxY;
                for (const auto& offset : offsets) {
                    const int x = cell.x + offset[0];
                    const int y = cell.y + offset[1];
                    if (x < bounds.minX || x > bounds.maxX ||
                        y < bounds.minY || y > bounds.maxY ||
                        isActive(x, y)) {
                        continue;
                    }
                    const std::size_t position =
                        static_cast<std::size_t>(localIndex(x, y));
                    if (visited[position]) {
                        continue;
                    }
                    visited[position] = 1;
                    queue.push({x, y});
                }
            }
            if (!touchesWindowBorder) {
                ++holes;
            }
        }
    }
    return holes;
}

int countLocalInactiveHoles(const BinaryMask& mask, CellBounds bounds) {
    return countInactiveHoles(
        mask.width(),
        mask.height(),
        bounds,
        [&mask](int x, int y) { return mask.get(x, y); });
}

class IncrementalComponentIndex {
public:
    explicit IncrementalComponentIndex(const BinaryMask& mask)
        : width_(mask.width()),
          height_(mask.height()),
          labels_(static_cast<std::size_t>(width_ * height_), 0) {
        constexpr int offsets[8][2] = {
            {-1, -1}, {0, -1}, {1, -1},
            {-1, 0},           {1, 0},
            {-1, 1},  {0, 1},  {1, 1}
        };
        for (int y0 = 0; y0 < height_; ++y0) {
            for (int x0 = 0; x0 < width_; ++x0) {
                const std::size_t start = position(x0, y0);
                if (!mask.get(x0, y0) || labels_[start] != 0) {
                    continue;
                }
                const int label = static_cast<int>(parent_.size());
                parent_.push_back(label);
                rank_.push_back(0);
                bounds_.push_back({});
                holeCache_.push_back(-1);
                std::queue<Cell> queue;
                queue.push({x0, y0});
                labels_[start] = label;
                while (!queue.empty()) {
                    const Cell cell = queue.front();
                    queue.pop();
                    bounds_[static_cast<std::size_t>(label)].include(
                        cell.x, cell.y);
                    for (const auto& offset : offsets) {
                        const int x = cell.x + offset[0];
                        const int y = cell.y + offset[1];
                        if (!mask.inBounds(x, y) || !mask.get(x, y)) {
                            continue;
                        }
                        const std::size_t next = position(x, y);
                        if (labels_[next] != 0) {
                            continue;
                        }
                        labels_[next] = label;
                        queue.push({x, y});
                    }
                }
            }
        }
    }

    int rootAt(int x, int y) {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            return 0;
        }
        return findRoot(labels_[position(x, y)]);
    }

    int componentHoles(int label) {
        const int root = findRoot(label);
        if (root <= 0) {
            return 0;
        }
        int& cached = holeCache_[static_cast<std::size_t>(root)];
        if (cached >= 0) {
            return cached;
        }
        CellBounds bounds = bounds_[static_cast<std::size_t>(root)];
        --bounds.minX;
        --bounds.minY;
        ++bounds.maxX;
        ++bounds.maxY;
        cached = countInactiveHoles(
            width_,
            height_,
            bounds,
            [&](int x, int y) { return rootAt(x, y) == root; });
        return cached;
    }

    int trialMergedHoles(
        int labelA,
        int labelB,
        const BinaryMask& bridgeMask,
        const CellBounds& bridgeBounds) {
        const int rootA = findRoot(labelA);
        const int rootB = findRoot(labelB);
        CellBounds bounds = bounds_[static_cast<std::size_t>(rootA)];
        bounds.include(bounds_[static_cast<std::size_t>(rootB)]);
        bounds.include(bridgeBounds);
        --bounds.minX;
        --bounds.minY;
        ++bounds.maxX;
        ++bounds.maxY;
        return countInactiveHoles(
            width_,
            height_,
            bounds,
            [&](int x, int y) {
                if (bridgeMask.get(x, y)) {
                    return true;
                }
                const int root = rootAt(x, y);
                return root == rootA || root == rootB;
            });
    }

    void commitMerge(
        int labelA,
        int labelB,
        const BinaryMask& bridgeMask,
        const CellBounds& bridgeBounds) {
        int rootA = findRoot(labelA);
        int rootB = findRoot(labelB);
        if (rootA != rootB) {
            if (rank_[static_cast<std::size_t>(rootA)] <
                rank_[static_cast<std::size_t>(rootB)]) {
                std::swap(rootA, rootB);
            }
            parent_[static_cast<std::size_t>(rootB)] = rootA;
            if (rank_[static_cast<std::size_t>(rootA)] ==
                rank_[static_cast<std::size_t>(rootB)]) {
                ++rank_[static_cast<std::size_t>(rootA)];
            }
            bounds_[static_cast<std::size_t>(rootA)].include(
                bounds_[static_cast<std::size_t>(rootB)]);
        }
        bounds_[static_cast<std::size_t>(rootA)].include(bridgeBounds);
        holeCache_[static_cast<std::size_t>(rootA)] = 0;
        for (int y = bridgeBounds.minY; y <= bridgeBounds.maxY; ++y) {
            for (int x = bridgeBounds.minX; x <= bridgeBounds.maxX; ++x) {
                if (bridgeMask.get(x, y)) {
                    labels_[position(x, y)] = rootA;
                }
            }
        }
    }

private:
    std::size_t position(int x, int y) const {
        return static_cast<std::size_t>(y * width_ + x);
    }

    int findRoot(int label) {
        if (label <= 0) {
            return 0;
        }
        int root = label;
        while (parent_[static_cast<std::size_t>(root)] != root) {
            root = parent_[static_cast<std::size_t>(root)];
        }
        while (parent_[static_cast<std::size_t>(label)] != label) {
            const int next = parent_[static_cast<std::size_t>(label)];
            parent_[static_cast<std::size_t>(label)] = root;
            label = next;
        }
        return root;
    }

    int width_ = 0;
    int height_ = 0;
    std::vector<int> labels_;
    // Label zero represents inactive cells, so the metadata arrays start there.
    std::vector<int> parent_{0};
    std::vector<unsigned char> rank_{0};
    std::vector<CellBounds> bounds_{CellBounds{}};
    std::vector<int> holeCache_{-1};
};

struct EndpointJunctionContext {
    int branchId = -1;
    double distanceToJunction = -1.0;
    int clusterId = -1;
    int armCount = 0;
    double confidence = -1.0;
    bool attributionAvailable = false;
    bool branchMatched = false;
    bool ambiguous = false;
};

using EndpointJunctionContextMap =
    std::map<std::pair<int, int>, EndpointJunctionContext>;

EndpointJunctionContextMap buildEndpointJunctionContexts(
    const Grid2D& grid,
    const SkeletonGraph& graph,
    double supportThreshold) {
    std::vector<const SkeletonBranch*> branches;
    branches.reserve(graph.branches.size());
    for (const SkeletonBranch& branch : graph.branches) {
        branches.push_back(&branch);
    }
    BranchAttributionOptions attributionOptions;
    attributionOptions.supportThreshold = supportThreshold;
    const BranchAttributionSummary attribution =
        analyzeBranchAttribution(grid, branches, attributionOptions);

    std::map<int, EndpointJunctionContext> attributionByBranch;
    for (const JunctionAttributionResult& junction : attribution.junctions) {
        std::set<int> branchIds;
        std::set<int> matchedBranchIds;
        for (const BranchAttributionPair& pair : junction.pairs) {
            branchIds.insert(pair.firstBranchId);
            branchIds.insert(pair.secondBranchId);
            matchedBranchIds.insert(pair.firstBranchId);
            matchedBranchIds.insert(pair.secondBranchId);
        }
        branchIds.insert(
            junction.unmatchedBranchIds.begin(),
            junction.unmatchedBranchIds.end());
        for (const int branchId : branchIds) {
            EndpointJunctionContext context;
            context.branchId = branchId;
            context.clusterId = junction.clusterId;
            context.armCount = static_cast<int>(branchIds.size());
            context.confidence = junction.confidence;
            context.attributionAvailable = true;
            context.branchMatched =
                matchedBranchIds.find(branchId) != matchedBranchIds.end();
            context.ambiguous = junction.ambiguous;
            attributionByBranch[branchId] = context;
        }
    }

    EndpointJunctionContextMap contexts;
    for (const SkeletonBranch& branch : graph.branches) {
        if (branch.points.size() < 2) {
            continue;
        }
        SkeletonPoint endpoint;
        bool hasEndpointToJunctionPath = false;
        if (!branch.startIsJunction && branch.endIsJunction) {
            endpoint = branch.points.front();
            hasEndpointToJunctionPath = true;
        } else if (branch.startIsJunction && !branch.endIsJunction) {
            endpoint = branch.points.back();
            hasEndpointToJunctionPath = true;
        }
        if (!hasEndpointToJunctionPath) {
            continue;
        }

        EndpointJunctionContext context;
        const auto attributionIt = attributionByBranch.find(branch.id);
        if (attributionIt != attributionByBranch.end()) {
            context = attributionIt->second;
        }
        context.branchId = branch.id;
        context.distanceToJunction = 0.0;
        for (std::size_t index = 1; index < branch.points.size(); ++index) {
            const SkeletonPoint& previous = branch.points[index - 1];
            const SkeletonPoint& current = branch.points[index];
            context.distanceToJunction += std::hypot(
                static_cast<double>(current.x - previous.x),
                static_cast<double>(current.y - previous.y));
        }
        contexts[{endpoint.x, endpoint.y}] = context;
    }
    return contexts;
}

void assignJunctionContext(
    const EndpointJunctionContextMap& contexts,
    const SkeletonPoint& endpoint,
    bool endpointA,
    WeakContinuityCandidate& candidate) {
    const auto it = contexts.find({endpoint.x, endpoint.y});
    if (it == contexts.end()) {
        return;
    }
    const EndpointJunctionContext& context = it->second;
    if (endpointA) {
        candidate.endpointBranchIdA = context.branchId;
        candidate.endpointJunctionDistanceA = context.distanceToJunction;
        candidate.junctionClusterIdA = context.clusterId;
        candidate.junctionArmCountA = context.armCount;
        candidate.junctionConfidenceA = context.confidence;
        candidate.junctionAttributionAvailableA = context.attributionAvailable;
        candidate.junctionBranchMatchedA = context.branchMatched;
        candidate.junctionAmbiguousA = context.ambiguous;
    } else {
        candidate.endpointBranchIdB = context.branchId;
        candidate.endpointJunctionDistanceB = context.distanceToJunction;
        candidate.junctionClusterIdB = context.clusterId;
        candidate.junctionArmCountB = context.armCount;
        candidate.junctionConfidenceB = context.confidence;
        candidate.junctionAttributionAvailableB = context.attributionAvailable;
        candidate.junctionBranchMatchedB = context.branchMatched;
        candidate.junctionAmbiguousB = context.ambiguous;
    }
}

bool hasNearbyAttributeSupport(
    const Grid2D& grid,
    int x,
    int y,
    double minimumValue,
    int radius) {
    for (int oy = -radius; oy <= radius; ++oy) {
        for (int ox = -radius; ox <= radius; ++ox) {
            const int nx = x + ox;
            const int ny = y + oy;
            if (!grid.inBounds(nx, ny)) {
                continue;
            }
            const double value = grid.value(nx, ny);
            if (std::isfinite(value) && value >= minimumValue) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

FinalBridgeGateResult applyFinalObjectBridgeGates(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const SkeletonGraph& finalGraph,
    WeakContinuityResult& pass,
    const FinalBridgeGateOptions& options) {
    const GateClock::time_point totalStart = GateClock::now();
    FinalBridgeGateResult result;
    result.mask = activeMask;

    GateClock::time_point stageStart = GateClock::now();
    const EndpointJunctionContextMap junctionContexts =
        buildEndpointJunctionContexts(
            grid, finalGraph, options.supportThreshold);
    result.timings.contextBuildMs = elapsedMilliseconds(stageStart);

    stageStart = GateClock::now();
    IncrementalComponentIndex owners(result.mask);
    result.timings.ownershipInitializationMs = elapsedMilliseconds(stageStart);
    pass.stats.acceptedConnections = 0;
    pass.stats.addedPathPixels = 0;

    std::vector<std::size_t> candidateOrder;
    candidateOrder.reserve(pass.candidates.size());
    for (std::size_t index = 0; index < pass.candidates.size(); ++index) {
        if (pass.candidates[index].accepted &&
            pass.candidates[index].path.size() >= 2) {
            candidateOrder.push_back(index);
        }
    }
    std::sort(
        candidateOrder.begin(),
        candidateOrder.end(),
        [&](std::size_t firstIndex, std::size_t secondIndex) {
            const WeakContinuityCandidate& first = pass.candidates[firstIndex];
            const WeakContinuityCandidate& second = pass.candidates[secondIndex];
            return std::make_tuple(
                       first.normalizedCost,
                       -first.supportRatio,
                       -first.meanSupport,
                       first.pathLength,
                       first.id) <
                std::make_tuple(
                       second.normalizedCost,
                       -second.supportRatio,
                       -second.meanSupport,
                       second.pathLength,
                       second.id);
        });

    for (const std::size_t candidateIndex : candidateOrder) {
        WeakContinuityCandidate& candidate = pass.candidates[candidateIndex];
        candidate.accepted = false;
        candidate.stage = 3;
        assignJunctionContext(
            junctionContexts, candidate.endpointA, true, candidate);
        assignJunctionContext(
            junctionContexts, candidate.endpointB, false, candidate);

        const auto junctionFailure = [&](bool endpointA) -> const char* {
            const double distanceToJunction = endpointA
                ? candidate.endpointJunctionDistanceA
                : candidate.endpointJunctionDistanceB;
            if (distanceToJunction < 0.0 ||
                distanceToJunction > options.junctionInfluenceDistance) {
                return nullptr;
            }
            const bool available = endpointA
                ? candidate.junctionAttributionAvailableA
                : candidate.junctionAttributionAvailableB;
            const bool matched = endpointA
                ? candidate.junctionBranchMatchedA
                : candidate.junctionBranchMatchedB;
            const bool ambiguous = endpointA
                ? candidate.junctionAmbiguousA
                : candidate.junctionAmbiguousB;
            if (!available) {
                return "final_bridge_unresolved_near_junction";
            }
            if (ambiguous) {
                return "final_bridge_ambiguous_junction_attribution";
            }
            if (!matched) {
                return "final_bridge_unmatched_junction_branch";
            }
            return nullptr;
        };
        if (const char* failure = junctionFailure(true)) {
            candidate.decision = failure;
            ++result.rejectedConnections;
            continue;
        }
        if (const char* failure = junctionFailure(false)) {
            candidate.decision = failure;
            ++result.rejectedConnections;
            continue;
        }

        const int endpointLabelA = owners.rootAt(
            candidate.endpointA.x, candidate.endpointA.y);
        const int endpointLabelB = owners.rootAt(
            candidate.endpointB.x, candidate.endpointB.y);
        if (endpointLabelA <= 0 || endpointLabelB <= 0) {
            candidate.decision = "final_bridge_missing_object_owner";
            ++result.rejectedConnections;
            continue;
        }
        if (endpointLabelA == endpointLabelB) {
            candidate.decision = "final_bridge_already_connected_by_prior_bridge";
            ++result.rejectedConnections;
            continue;
        }

        stageStart = GateClock::now();
        candidate.endpointComponentHolesA =
            owners.componentHoles(endpointLabelA);
        candidate.endpointComponentHolesB =
            owners.componentHoles(endpointLabelB);
        result.timings.componentHoleCheckMs += elapsedMilliseconds(stageStart);
        if (candidate.endpointComponentHolesA > 0 ||
            candidate.endpointComponentHolesB > 0) {
            candidate.decision = "final_bridge_endpoint_component_has_internal_hole";
            ++result.rejectedConnections;
            continue;
        }

        const int beltRadius = std::clamp(
            static_cast<int>(std::lround(
                0.5 * (candidate.widthA + candidate.widthB))),
            1,
            std::max(1, options.maximumBeltRadius));
        const int requiredThirdObjectClearance = beltRadius + 1;
        const int thirdObjectSearchRadius = requiredThirdObjectClearance + 1;
        double minimumThirdObjectClearance =
            std::numeric_limits<double>::infinity();
        std::set<int> nearbyThirdObjectLabels;
        for (const SkeletonPoint& point : candidate.path) {
            for (int oy = -thirdObjectSearchRadius;
                 oy <= thirdObjectSearchRadius; ++oy) {
                for (int ox = -thirdObjectSearchRadius;
                     ox <= thirdObjectSearchRadius; ++ox) {
                    const int x = point.x + ox;
                    const int y = point.y + oy;
                    if (!result.mask.inBounds(x, y)) {
                        continue;
                    }
                    const int label = owners.rootAt(x, y);
                    if (label <= 0 || label == endpointLabelA ||
                        label == endpointLabelB) {
                        continue;
                    }
                    const double clearance = std::hypot(
                        static_cast<double>(ox),
                        static_cast<double>(oy));
                    if (clearance <= thirdObjectSearchRadius + 1e-9) {
                        minimumThirdObjectClearance = std::min(
                            minimumThirdObjectClearance, clearance);
                        nearbyThirdObjectLabels.insert(label);
                    }
                }
            }
        }
        candidate.nearbyThirdObjectCount =
            static_cast<int>(nearbyThirdObjectLabels.size());
        if (std::isfinite(minimumThirdObjectClearance)) {
            candidate.minimumThirdObjectClearance =
                minimumThirdObjectClearance;
        }
        if (minimumThirdObjectClearance <=
            requiredThirdObjectClearance + 1e-9) {
            candidate.decision =
                "final_bridge_insufficient_third_object_clearance";
            ++result.rejectedConnections;
            continue;
        }

        stageStart = GateClock::now();
        BinaryMask bridgeMask(grid.nx(), grid.ny());
        CellBounds pathBounds;
        for (const SkeletonPoint& point : candidate.path) {
            pathBounds.include(point.x, point.y);
            for (int oy = -beltRadius; oy <= beltRadius; ++oy) {
                for (int ox = -beltRadius; ox <= beltRadius; ++ox) {
                    if (ox * ox + oy * oy > beltRadius * beltRadius) {
                        continue;
                    }
                    const int x = point.x + ox;
                    const int y = point.y + oy;
                    if (!grid.inBounds(x, y)) {
                        continue;
                    }
                    if (result.mask.get(x, y) || hasNearbyAttributeSupport(
                            grid,
                            x,
                            y,
                            candidate.minimumSupportedValue,
                            std::max(0, options.supportEnvelopeRadius))) {
                        bridgeMask.set(x, y, true);
                    }
                }
            }
            bridgeMask.set(point.x, point.y, true);
        }

        const int contactPadding = beltRadius + 1;
        CellBounds bridgeBounds{
            std::max(0, pathBounds.minX - contactPadding),
            std::max(0, pathBounds.minY - contactPadding),
            std::min(grid.nx() - 1, pathBounds.maxX + contactPadding),
            std::min(grid.ny() - 1, pathBounds.maxY + contactPadding)};
        std::set<int> contactedLabels;
        int addedCells = 0;
        for (int y = bridgeBounds.minY; y <= bridgeBounds.maxY; ++y) {
            for (int x = bridgeBounds.minX; x <= bridgeBounds.maxX; ++x) {
                if (bridgeMask.get(x, y) && !result.mask.get(x, y)) {
                    ++addedCells;
                }
                if (!bridgeMask.get(x, y)) {
                    continue;
                }
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        const int label = owners.rootAt(x + ox, y + oy);
                        if (label > 0) {
                            contactedLabels.insert(label);
                        }
                    }
                }
            }
        }
        result.timings.envelopeBuildMs += elapsedMilliseconds(stageStart);
        candidate.contactedObjectCount =
            static_cast<int>(contactedLabels.size());
        const bool hasEndpointA =
            contactedLabels.find(endpointLabelA) != contactedLabels.end();
        const bool hasEndpointB =
            contactedLabels.find(endpointLabelB) != contactedLabels.end();
        if (contactedLabels.size() != 2 || !hasEndpointA || !hasEndpointB) {
            candidate.decision = "final_bridge_third_object_or_owner_mismatch";
            ++result.rejectedConnections;
            continue;
        }
        if (addedCells <= 0) {
            candidate.decision = "final_bridge_no_new_envelope";
            ++result.rejectedConnections;
            continue;
        }
        const int maximumAddedCells = static_cast<int>(std::ceil(
            std::max(1.0, candidate.pathLength) *
            static_cast<double>(2 * beltRadius + 1) * 1.5)) + 8;
        if (addedCells > maximumAddedCells) {
            candidate.decision = "final_bridge_envelope_growth_too_large";
            ++result.rejectedConnections;
            continue;
        }

        stageStart = GateClock::now();
        const int topologyPadding = beltRadius + 4;
        const CellBounds topologyBounds{
            bridgeBounds.minX - topologyPadding,
            bridgeBounds.minY - topologyPadding,
            bridgeBounds.maxX + topologyPadding,
            bridgeBounds.maxY + topologyPadding};
        const int holesBefore = countLocalInactiveHoles(
            result.mask, topologyBounds);
        BinaryMask trial = result.mask;
        for (int y = bridgeBounds.minY; y <= bridgeBounds.maxY; ++y) {
            for (int x = bridgeBounds.minX; x <= bridgeBounds.maxX; ++x) {
                if (bridgeMask.get(x, y)) {
                    trial.set(x, y, true);
                }
            }
        }
        const int holesAfter = countLocalInactiveHoles(trial, topologyBounds);
        candidate.localHolesBefore = holesBefore;
        candidate.localHolesAfter = holesAfter;
        if (holesAfter > holesBefore) {
            result.timings.localTopologyCheckMs += elapsedMilliseconds(stageStart);
            candidate.decision = "final_bridge_new_internal_hole";
            ++result.rejectedConnections;
            continue;
        }
        candidate.mergedComponentHolesAfter = owners.trialMergedHoles(
            endpointLabelA, endpointLabelB, bridgeMask, bridgeBounds);
        result.timings.localTopologyCheckMs += elapsedMilliseconds(stageStart);
        if (candidate.mergedComponentHolesAfter > 0) {
            candidate.decision = "final_bridge_merged_component_has_internal_hole";
            ++result.rejectedConnections;
            continue;
        }

        result.mask = std::move(trial);
        owners.commitMerge(
            endpointLabelA, endpointLabelB, bridgeMask, bridgeBounds);
        candidate.addedEnvelopeCells = addedCells;
        candidate.accepted = true;
        candidate.decision = "accepted_final_object_bridge";
        ++result.acceptedConnections;
        result.addedEnvelopeCells += addedCells;
        ++pass.stats.acceptedConnections;
        pass.stats.addedPathPixels += candidate.addedPathPixels;
    }

    result.timings.totalMs = elapsedMilliseconds(totalStart);
    return result;
}

} // namespace fpt
