#include "BranchAttribution.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace fpt {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-12;

double clampUnit(double value) {
    return std::clamp(value, 0.0, 1.0);
}

double vectorNorm(double x, double y) {
    return std::hypot(x, y);
}

double angleDegrees(double ax, double ay, double bx, double by) {
    const double denominator = vectorNorm(ax, ay) * vectorNorm(bx, by);
    if (denominator <= kEpsilon) {
        return 0.0;
    }
    const double cosine = std::clamp(
        (ax * bx + ay * by) / denominator,
        -1.0,
        1.0);
    return std::acos(cosine) * 180.0 / kPi;
}

struct PairEvidence {
    bool allowed = false;
    double cost = std::numeric_limits<double>::infinity();
    double turnAngleDegrees = 180.0;
    double angleCost = 1.0;
    double attributeCost = 1.0;
    double widthCost = 1.0;
    double backgroundCost = 1.0;
};

PairEvidence evaluatePair(
    const BranchAttributionEnd& first,
    const BranchAttributionEnd& second,
    const BranchAttributionOptions& options) {
    PairEvidence evidence;
    const double outwardAngle = angleDegrees(
        first.directionX,
        first.directionY,
        second.directionX,
        second.directionY);
    evidence.turnAngleDegrees = std::abs(180.0 - outwardAngle);
    if (evidence.turnAngleDegrees > options.maxTurnAngleDegrees) {
        return evidence;
    }

    evidence.angleCost = evidence.turnAngleDegrees / 180.0;
    const double firstSupport = clampUnit(first.meanSupport);
    const double secondSupport = clampUnit(second.meanSupport);
    evidence.attributeCost =
        0.5 * std::abs(firstSupport - secondSupport) +
        0.5 * (1.0 - std::min(firstSupport, secondSupport));
    const double widthDenominator = std::max(
        {std::abs(first.localWidth), std::abs(second.localWidth), 1.0});
    evidence.widthCost =
        std::abs(first.localWidth - second.localWidth) / widthDenominator;
    evidence.backgroundCost =
        1.0 - std::min(
            clampUnit(first.backgroundIsolation),
            clampUnit(second.backgroundIsolation));
    const double weightSum =
        options.angleWeight +
        options.attributeWeight +
        options.widthWeight +
        options.backgroundWeight;
    if (weightSum <= kEpsilon) {
        throw std::runtime_error(
            "Branch attribution requires at least one positive pair weight.");
    }
    evidence.cost = (
        options.angleWeight * evidence.angleCost +
        options.attributeWeight * evidence.attributeCost +
        options.widthWeight * evidence.widthCost +
        options.backgroundWeight * evidence.backgroundCost) /
        weightSum;
    evidence.allowed = evidence.cost <= options.maxPairCost;
    return evidence;
}

double unmatchedCost(
    const BranchAttributionEnd& end,
    const BranchAttributionOptions& options) {
    const double reliability =
        0.25 + 0.75 * clampUnit(end.meanSupport);
    return options.unmatchedCost * reliability;
}

struct MatchingSolution {
    double cost = 0.0;
    std::vector<std::pair<int, int>> pairs;
    std::vector<int> unmatched;
};

void normalizeSolution(MatchingSolution& solution) {
    for (auto& pair : solution.pairs) {
        if (pair.second < pair.first) {
            std::swap(pair.first, pair.second);
        }
    }
    std::sort(solution.pairs.begin(), solution.pairs.end());
    std::sort(solution.unmatched.begin(), solution.unmatched.end());
}

bool solutionLess(
    const MatchingSolution& first,
    const MatchingSolution& second) {
    if (std::abs(first.cost - second.cost) > kEpsilon) {
        return first.cost < second.cost;
    }
    if (first.pairs != second.pairs) {
        return first.pairs < second.pairs;
    }
    return first.unmatched < second.unmatched;
}

std::vector<MatchingSolution> enumerateMatchings(
    const std::vector<BranchAttributionEnd>& ends,
    const std::vector<std::vector<PairEvidence>>& pairEvidence,
    const BranchAttributionOptions& options) {
    std::vector<MatchingSolution> solutions;
    std::vector<unsigned char> used(ends.size(), 0);
    MatchingSolution current;

    std::function<void()> search = [&]() {
        std::size_t firstUnused = ends.size();
        for (std::size_t index = 0; index < ends.size(); ++index) {
            if (!used[index]) {
                firstUnused = index;
                break;
            }
        }
        if (firstUnused == ends.size()) {
            MatchingSolution solution = current;
            normalizeSolution(solution);
            solutions.push_back(std::move(solution));
            return;
        }

        used[firstUnused] = 1;
        current.unmatched.push_back(ends[firstUnused].branchId);
        current.cost += unmatchedCost(ends[firstUnused], options);
        search();
        current.cost -= unmatchedCost(ends[firstUnused], options);
        current.unmatched.pop_back();

        for (std::size_t second = firstUnused + 1;
             second < ends.size();
             ++second) {
            if (used[second] ||
                ends[firstUnused].branchId == ends[second].branchId ||
                !pairEvidence[firstUnused][second].allowed) {
                continue;
            }
            used[second] = 1;
            current.pairs.push_back({
                ends[firstUnused].branchId,
                ends[second].branchId});
            current.cost += pairEvidence[firstUnused][second].cost;
            search();
            current.cost -= pairEvidence[firstUnused][second].cost;
            current.pairs.pop_back();
            used[second] = 0;
        }
        used[firstUnused] = 0;
    };
    search();
    std::sort(solutions.begin(), solutions.end(), solutionLess);
    return solutions;
}

MatchingSolution greedyMatching(
    const std::vector<BranchAttributionEnd>& ends,
    const std::vector<std::vector<PairEvidence>>& pairEvidence,
    const BranchAttributionOptions& options) {
    MatchingSolution result;
    std::vector<unsigned char> used(ends.size(), 0);
    for (std::size_t first = 0; first < ends.size(); ++first) {
        if (used[first]) {
            continue;
        }
        int bestSecond = -1;
        double bestPairCost = std::numeric_limits<double>::infinity();
        for (std::size_t second = first + 1;
             second < ends.size();
             ++second) {
            if (used[second] ||
                ends[first].branchId == ends[second].branchId ||
                !pairEvidence[first][second].allowed) {
                continue;
            }
            const double cost = pairEvidence[first][second].cost;
            if (cost < bestPairCost - kEpsilon ||
                (std::abs(cost - bestPairCost) <= kEpsilon &&
                 ends[second].branchId <
                     ends[static_cast<std::size_t>(
                         std::max(bestSecond, 0))].branchId)) {
                bestPairCost = cost;
                bestSecond = static_cast<int>(second);
            }
        }
        const double leaveBothUnmatched =
            unmatchedCost(ends[first], options) +
            (bestSecond >= 0
                ? unmatchedCost(
                    ends[static_cast<std::size_t>(bestSecond)],
                    options)
                : 0.0);
        if (bestSecond >= 0 && bestPairCost <= leaveBothUnmatched) {
            const std::size_t second =
                static_cast<std::size_t>(bestSecond);
            used[first] = 1;
            used[second] = 1;
            result.pairs.push_back({
                ends[first].branchId,
                ends[second].branchId});
            result.cost += bestPairCost;
        } else {
            used[first] = 1;
            result.unmatched.push_back(ends[first].branchId);
            result.cost += unmatchedCost(ends[first], options);
        }
    }
    normalizeSolution(result);
    return result;
}

class DisjointSet {
public:
    explicit DisjointSet(int size)
        : parent_(static_cast<std::size_t>(size)) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int value) {
        int& parent = parent_[static_cast<std::size_t>(value)];
        if (parent != value) {
            parent = find(parent);
        }
        return parent;
    }

    void unite(int first, int second) {
        first = find(first);
        second = find(second);
        if (first != second) {
            parent_[static_cast<std::size_t>(second)] = first;
        }
    }

private:
    std::vector<int> parent_;
};

class LegacyDisjointSet {
public:
    explicit LegacyDisjointSet(int size)
        : parent_(static_cast<std::size_t>(size)),
          rank_(static_cast<std::size_t>(size), 0) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    int find(int value) {
        int& parent = parent_[static_cast<std::size_t>(value)];
        if (parent != value) {
            parent = find(parent);
        }
        return parent;
    }

    void unite(int first, int second) {
        int firstRoot = find(first);
        int secondRoot = find(second);
        if (firstRoot == secondRoot) {
            return;
        }
        if (rank_[static_cast<std::size_t>(firstRoot)] <
            rank_[static_cast<std::size_t>(secondRoot)]) {
            std::swap(firstRoot, secondRoot);
        }
        parent_[static_cast<std::size_t>(secondRoot)] = firstRoot;
        if (rank_[static_cast<std::size_t>(firstRoot)] ==
            rank_[static_cast<std::size_t>(secondRoot)]) {
            ++rank_[static_cast<std::size_t>(firstRoot)];
        }
    }

private:
    std::vector<int> parent_;
    std::vector<int> rank_;
};

struct RawBranchEnd {
    const SkeletonBranch* branch = nullptr;
    SkeletonPoint junction;
    std::vector<SkeletonPoint> pointsFromJunction;
};

std::vector<std::pair<int, int>> normalizedPairIds(
    const std::vector<BranchAttributionPair>& pairs) {
    std::vector<std::pair<int, int>> normalized;
    normalized.reserve(pairs.size());
    for (const BranchAttributionPair& pair : pairs) {
        normalized.push_back({
            std::min(pair.firstBranchId, pair.secondBranchId),
            std::max(pair.firstBranchId, pair.secondBranchId)});
    }
    std::sort(normalized.begin(), normalized.end());
    normalized.erase(
        std::unique(normalized.begin(), normalized.end()),
        normalized.end());
    return normalized;
}

std::vector<int> normalizedIds(std::vector<int> ids) {
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

bool hasSameExternalConnectivity(
    const std::vector<int>& externalBranchIds,
    const std::vector<BranchAttributionPair>& legacyPairs,
    const std::vector<BranchAttributionPair>& modernPairs) {
    std::set<int> allIds(
        externalBranchIds.begin(),
        externalBranchIds.end());
    for (const BranchAttributionPair& pair : legacyPairs) {
        allIds.insert(pair.firstBranchId);
        allIds.insert(pair.secondBranchId);
    }
    for (const BranchAttributionPair& pair : modernPairs) {
        allIds.insert(pair.firstBranchId);
        allIds.insert(pair.secondBranchId);
    }
    std::map<int, int> indexById;
    int nextIndex = 0;
    for (const int branchId : allIds) {
        indexById[branchId] = nextIndex++;
    }
    DisjointSet legacySet(nextIndex);
    DisjointSet modernSet(nextIndex);
    for (const BranchAttributionPair& pair : legacyPairs) {
        legacySet.unite(
            indexById.at(pair.firstBranchId),
            indexById.at(pair.secondBranchId));
    }
    for (const BranchAttributionPair& pair : modernPairs) {
        modernSet.unite(
            indexById.at(pair.firstBranchId),
            indexById.at(pair.secondBranchId));
    }
    for (std::size_t first = 0;
         first < externalBranchIds.size();
         ++first) {
        for (std::size_t second = first + 1;
             second < externalBranchIds.size();
             ++second) {
            const int firstIndex =
                indexById.at(externalBranchIds[first]);
            const int secondIndex =
                indexById.at(externalBranchIds[second]);
            const bool legacyConnected =
                legacySet.find(firstIndex) ==
                legacySet.find(secondIndex);
            const bool modernConnected =
                modernSet.find(firstIndex) ==
                modernSet.find(secondIndex);
            if (legacyConnected != modernConnected) {
                return false;
            }
        }
    }
    return true;
}

void appendLegacyGreedyEvidence(
    const std::vector<RawBranchEnd>& rawEnds,
    const std::vector<std::size_t>& group,
    double maxTurnAngleDegrees,
    JunctionAttributionResult& junction) {
    std::set<std::pair<int, int>> junctionPoints;
    std::set<int> branchIds;
    std::set<const SkeletonBranch*> seenBranches;
    std::vector<const SkeletonBranch*> branches;
    for (const std::size_t index : group) {
        const RawBranchEnd& raw = rawEnds[index];
        if (!raw.branch || raw.pointsFromJunction.size() < 2) {
            continue;
        }
        junctionPoints.insert({raw.junction.x, raw.junction.y});
        branchIds.insert(raw.branch->id);
        if (seenBranches.insert(raw.branch).second) {
            branches.push_back(raw.branch);
        }
    }

    const LegacyGreedyBranchPairingResult legacy =
        computeLegacyGreedyBranchPairing(
            branches,
            maxTurnAngleDegrees);
    std::set<int> pairedBranchIds;
    for (const LegacyGreedyBranchPair& pair : legacy.pairs) {
        if (junctionPoints.find(
                {pair.junction.x, pair.junction.y}) ==
            junctionPoints.end()) {
            continue;
        }
        pairedBranchIds.insert(pair.firstBranchId);
        pairedBranchIds.insert(pair.secondBranchId);
        junction.legacyPairs.push_back({
            pair.firstBranchId,
            pair.secondBranchId,
            pair.turnAngleDegrees / 180.0,
            pair.turnAngleDegrees,
            pair.turnAngleDegrees / 180.0,
            0.0,
            0.0,
            0.0});
    }

    for (const int branchId : branchIds) {
        if (pairedBranchIds.find(branchId) == pairedBranchIds.end()) {
            junction.legacyUnmatchedBranchIds.push_back(branchId);
        }
    }
    junction.legacyUnmatchedBranchIds =
        normalizedIds(std::move(junction.legacyUnmatchedBranchIds));
}

double normalizedSupport(
    double value,
    double threshold) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
    if (threshold > kEpsilon) {
        return clampUnit(value / threshold);
    }
    return clampUnit(value);
}

BranchAttributionEnd buildEndEvidence(
    const Grid2D& grid,
    const RawBranchEnd& raw,
    const BranchAttributionOptions& options) {
    BranchAttributionEnd end;
    end.branchId = raw.branch ? raw.branch->id : -1;
    const BranchDirection direction =
        estimateMultiScaleBranchDirection(
            grid,
            raw.pointsFromJunction,
            options.directionWindows);
    end.directionX = direction.x;
    end.directionY = direction.y;

    const int sampleCount = std::min(
        static_cast<int>(raw.pointsFromJunction.size()),
        std::max(1, options.attributeSampleCells));
    double supportSum = 0.0;
    int finiteCount = 0;
    for (int index = 0; index < sampleCount; ++index) {
        const SkeletonPoint& point =
            raw.pointsFromJunction[static_cast<std::size_t>(index)];
        const double value = grid.value(point.x, point.y);
        if (!std::isfinite(value)) {
            continue;
        }
        supportSum += normalizedSupport(
            value,
            options.supportThreshold);
        ++finiteCount;
    }
    end.meanSupport = finiteCount > 0
        ? supportSum / static_cast<double>(finiteCount)
        : 0.0;

    if (!direction.valid ||
        options.supportThreshold <= kEpsilon ||
        sampleCount <= 0) {
        return end;
    }

    double cellDirectionX = direction.x / grid.dx();
    double cellDirectionY = direction.y / grid.dy();
    const double cellNorm =
        vectorNorm(cellDirectionX, cellDirectionY);
    if (cellNorm <= kEpsilon) {
        return end;
    }
    cellDirectionX /= cellNorm;
    cellDirectionY /= cellNorm;
    const double normalX = -cellDirectionY;
    const double normalY = cellDirectionX;
    double widthSum = 0.0;
    double isolationSum = 0.0;
    int measured = 0;
    for (int index = 0; index < sampleCount; ++index) {
        const SkeletonPoint& point =
            raw.pointsFromJunction[static_cast<std::size_t>(index)];
        int negativeWidth = 0;
        int positiveWidth = 0;
        for (int sign : {-1, 1}) {
            int sideWidth = 0;
            for (int radius = 1;
                 radius <= options.widthSampleRadius;
                 ++radius) {
                const int sampleX = static_cast<int>(std::lround(
                    static_cast<double>(point.x) +
                    sign * normalX * radius));
                const int sampleY = static_cast<int>(std::lround(
                    static_cast<double>(point.y) +
                    sign * normalY * radius));
                if (!grid.inBounds(sampleX, sampleY) ||
                    !std::isfinite(grid.value(sampleX, sampleY)) ||
                    grid.value(sampleX, sampleY) <
                        options.supportThreshold) {
                    break;
                }
                sideWidth = radius;
            }
            if (sign < 0) {
                negativeWidth = sideWidth;
            } else {
                positiveWidth = sideWidth;
            }
        }
        const int width =
            1 + negativeWidth + positiveWidth;
        widthSum += static_cast<double>(width);

        int isolatedSides = 0;
        for (int sign : {-1, 1}) {
            const int radius =
                (sign < 0 ? negativeWidth : positiveWidth) + 1;
            const int sampleX = static_cast<int>(std::lround(
                static_cast<double>(point.x) +
                sign * normalX * radius));
            const int sampleY = static_cast<int>(std::lround(
                static_cast<double>(point.y) +
                sign * normalY * radius));
            if (!grid.inBounds(sampleX, sampleY) ||
                !std::isfinite(grid.value(sampleX, sampleY)) ||
                grid.value(sampleX, sampleY) <
                    options.supportThreshold) {
                ++isolatedSides;
            }
        }
        isolationSum +=
            static_cast<double>(isolatedSides) / 2.0;
        ++measured;
    }
    if (measured > 0) {
        end.localWidth =
            widthSum / static_cast<double>(measured);
        end.backgroundIsolation =
            isolationSum / static_cast<double>(measured);
    }
    return end;
}

} // namespace

LegacyGreedyBranchPairingResult computeLegacyGreedyBranchPairing(
    const std::vector<const SkeletonBranch*>& branches,
    double maxTurnAngleDegrees) {
    struct LegacyBranchEnd {
        int branchIndex = -1;
        SkeletonPoint direction;
    };

    LegacyGreedyBranchPairingResult result;
    if (branches.empty()) {
        return result;
    }

    // Preserve the released legacy traversal exactly. The deterministic
    // multi-scale alternative runs in the shadow path and must not silently
    // change production root ownership before promotion gates pass.
    std::unordered_map<std::string, std::vector<LegacyBranchEnd>>
        endsByPoint;
    endsByPoint.reserve(branches.size() * 2);
    const auto pointKey = [](const SkeletonPoint& point) {
        return std::to_string(point.x) + "," +
            std::to_string(point.y);
    };
    for (std::size_t index = 0; index < branches.size(); ++index) {
        const SkeletonBranch* branch = branches[index];
        if (!branch || branch->points.size() < 2) {
            continue;
        }
        const SkeletonPoint& first = branch->points.front();
        const SkeletonPoint& second = branch->points[1];
        endsByPoint[pointKey(first)].push_back({
            static_cast<int>(index),
            {second.x - first.x, second.y - first.y}});

        const SkeletonPoint& last = branch->points.back();
        const SkeletonPoint& beforeLast =
            branch->points[branch->points.size() - 2];
        endsByPoint[pointKey(last)].push_back({
            static_cast<int>(index),
            {beforeLast.x - last.x, beforeLast.y - last.y}});
    }

    LegacyDisjointSet roots(static_cast<int>(branches.size()));
    for (const auto& item : endsByPoint) {
        const std::vector<LegacyBranchEnd>& ends = item.second;
        if (ends.size() < 2) {
            continue;
        }
        std::vector<unsigned char> used(ends.size(), 0);
        for (std::size_t first = 0; first < ends.size(); ++first) {
            if (used[first]) {
                continue;
            }
            int bestSecond = -1;
            double bestAngle = maxTurnAngleDegrees;
            for (std::size_t second = first + 1;
                 second < ends.size();
                 ++second) {
                if (used[second] ||
                    ends[first].branchIndex ==
                        ends[second].branchIndex) {
                    continue;
                }
                const SkeletonPoint opposite{
                    -ends[second].direction.x,
                    -ends[second].direction.y};
                const SkeletonPoint& firstDirection =
                    ends[first].direction;
                const double denominator =
                    std::sqrt(static_cast<double>(
                        firstDirection.x * firstDirection.x +
                        firstDirection.y * firstDirection.y)) *
                    std::sqrt(static_cast<double>(
                        opposite.x * opposite.x +
                        opposite.y * opposite.y));
                double angle = 0.0;
                if (denominator > kEpsilon) {
                    double cosine = static_cast<double>(
                        firstDirection.x * opposite.x +
                        firstDirection.y * opposite.y) /
                        denominator;
                    cosine = std::max(
                        -1.0,
                        std::min(1.0, cosine));
                    angle = std::acos(cosine) * 180.0 / kPi;
                }
                if (angle <= bestAngle) {
                    bestAngle = angle;
                    bestSecond = static_cast<int>(second);
                }
            }
            if (bestSecond < 0) {
                continue;
            }
            const std::size_t second =
                static_cast<std::size_t>(bestSecond);
            const int firstBranchIndex =
                ends[first].branchIndex;
            const int secondBranchIndex =
                ends[second].branchIndex;
            used[first] = 1;
            used[second] = 1;
            roots.unite(firstBranchIndex, secondBranchIndex);
            result.pairs.push_back({
                firstBranchIndex,
                secondBranchIndex,
                branches[static_cast<std::size_t>(
                    firstBranchIndex)]->id,
                branches[static_cast<std::size_t>(
                    secondBranchIndex)]->id,
                branches[static_cast<std::size_t>(
                    firstBranchIndex)]->points.front(),
                bestAngle});
            const SkeletonBranch* firstBranch =
                branches[static_cast<std::size_t>(
                    firstBranchIndex)];
            const SkeletonPoint& firstPoint =
                firstBranch->points.front();
            const SkeletonPoint& lastPoint =
                firstBranch->points.back();
            if (item.first == pointKey(lastPoint) &&
                item.first != pointKey(firstPoint)) {
                result.pairs.back().junction = lastPoint;
            }
        }
    }

    std::map<int, std::vector<int>> grouped;
    for (std::size_t index = 0; index < branches.size(); ++index) {
        grouped[roots.find(static_cast<int>(index))].push_back(
            static_cast<int>(index));
    }
    result.rootBranchIndices.reserve(grouped.size());
    for (auto& item : grouped) {
        // Root IDs are implementation details of the union-find structure.
        // Canonicalize the exported groups by their member branch indices so
        // downstream ownership propagation does not depend on hash-map or
        // union-by-rank traversal order across compilers.
        std::sort(item.second.begin(), item.second.end());
        result.rootBranchIndices.push_back(std::move(item.second));
    }
    std::sort(
        result.rootBranchIndices.begin(),
        result.rootBranchIndices.end(),
        [](const std::vector<int>& first, const std::vector<int>& second) {
            if (first.empty() || second.empty()) {
                return first.size() < second.size();
            }
            if (first.front() != second.front()) {
                return first.front() > second.front();
            }
            return first > second;
        });
    return result;
}

CandidateRootGroupingResult computeCandidateRootGrouping(
    const std::vector<const SkeletonBranch*>& branches,
    const std::vector<JunctionAttributionResult>& junctions,
    const CandidateRootGroupingOptions& options) {
    CandidateRootGroupingResult result;
    if (branches.empty()) {
        return result;
    }
    if (!std::isfinite(options.maxTurnAngleDegrees) ||
        options.maxTurnAngleDegrees < 0.0 ||
        options.maxTurnAngleDegrees > 180.0 ||
        options.maxJunctionSpanCells < 0 ||
        options.maxInternalBranchCount < 0 ||
        !std::isfinite(options.maxInternalToExternalRatio) ||
        options.maxInternalToExternalRatio < 0.0) {
        throw std::runtime_error(
            "Invalid candidate root grouping options.");
    }

    std::unordered_map<int, int> indexById;
    indexById.reserve(branches.size());
    for (std::size_t index = 0; index < branches.size(); ++index) {
        const SkeletonBranch* branch = branches[index];
        if (!branch) {
            throw std::runtime_error(
                "Candidate root grouping requires non-null branches.");
        }
        const auto inserted = indexById.emplace(
            branch->id,
            static_cast<int>(index));
        if (!inserted.second) {
            throw std::runtime_error(
                "Candidate root grouping requires unique branch ids.");
        }
    }

    const LegacyGreedyBranchPairingResult formalPairing =
        computeLegacyGreedyBranchPairing(
            branches,
            options.maxTurnAngleDegrees);
    std::vector<unsigned char> applyCandidate(
        junctions.size(),
        0);
    std::vector<std::set<int>> internalIdsByJunction(
        junctions.size());
    std::set<std::pair<int, int>> changedJunctionPoints;
    for (std::size_t junctionIndex = 0;
         junctionIndex < junctions.size();
         ++junctionIndex) {
        const JunctionAttributionResult& junction =
            junctions[junctionIndex];
        if (!junction.acceptedDiffersFromLegacy ||
            !junction.usedExactMatching) {
            continue;
        }
        std::set<int> externalBranchIds;
        for (const BranchAttributionPair& pair : junction.pairs) {
            externalBranchIds.insert(pair.firstBranchId);
            externalBranchIds.insert(pair.secondBranchId);
        }
        externalBranchIds.insert(
            junction.unmatchedBranchIds.begin(),
            junction.unmatchedBranchIds.end());
        std::set<int>& internalBranchIds =
            internalIdsByJunction[junctionIndex];
        for (const BranchAttributionPair& pair : junction.legacyPairs) {
            for (const int branchId :
                 {pair.firstBranchId, pair.secondBranchId}) {
                if (externalBranchIds.find(branchId) ==
                    externalBranchIds.end()) {
                    internalBranchIds.insert(branchId);
                }
            }
        }
        for (const int branchId :
             junction.legacyUnmatchedBranchIds) {
            if (externalBranchIds.find(branchId) ==
                externalBranchIds.end()) {
                internalBranchIds.insert(branchId);
            }
        }
        const int span = std::max(
            junction.maxGridX - junction.minGridX,
            junction.maxGridY - junction.minGridY);
        if (span > options.maxJunctionSpanCells) {
            ++result.spanFallbackClusterCount;
            continue;
        }
        const double internalRatio =
            externalBranchIds.empty()
            ? std::numeric_limits<double>::infinity()
            : static_cast<double>(internalBranchIds.size()) /
                static_cast<double>(externalBranchIds.size());
        if (static_cast<int>(internalBranchIds.size()) >
                options.maxInternalBranchCount ||
            internalRatio > options.maxInternalToExternalRatio) {
            ++result.complexityFallbackClusterCount;
            continue;
        }
        applyCandidate[junctionIndex] = 1;
        for (const SkeletonPoint& point : junction.junctionPoints) {
            changedJunctionPoints.insert({point.x, point.y});
        }
    }

    DisjointSet roots(static_cast<int>(branches.size()));
    std::vector<unsigned char> excluded(branches.size(), 0);
    const auto requireBranchIndex = [&](int branchId) {
        const auto found = indexById.find(branchId);
        if (found == indexById.end()) {
            throw std::runtime_error(
                "Candidate root grouping references an unknown branch id.");
        }
        return found->second;
    };
    const auto unitePairs =
        [&](const std::vector<BranchAttributionPair>& pairs) {
            for (const BranchAttributionPair& pair : pairs) {
                roots.unite(
                    requireBranchIndex(pair.firstBranchId),
                    requireBranchIndex(pair.secondBranchId));
            }
        };

    for (const LegacyGreedyBranchPair& pair : formalPairing.pairs) {
        if (changedJunctionPoints.find(
                {pair.junction.x, pair.junction.y}) !=
            changedJunctionPoints.end()) {
            continue;
        }
        roots.unite(pair.firstBranchIndex, pair.secondBranchIndex);
    }

    for (std::size_t junctionIndex = 0;
         junctionIndex < junctions.size();
         ++junctionIndex) {
        const JunctionAttributionResult& junction =
            junctions[junctionIndex];
        if (!applyCandidate[junctionIndex]) {
            unitePairs(junction.legacyPairs);
            if (junction.ambiguous && junction.differsFromLegacy) {
                ++result.ambiguousFallbackClusterCount;
            } else if ((!junction.acceptedDiffersFromLegacy ||
                !junction.usedExactMatching) &&
                (
                junction.pairingDiffersFromLegacy ||
                junction.differsFromLegacy)) {
                ++result.legacyFallbackClusterCount;
            }
            continue;
        }

        for (const int branchId :
             internalIdsByJunction[junctionIndex]) {
            excluded[static_cast<std::size_t>(
                requireBranchIndex(branchId))] = 1;
        }
        unitePairs(junction.pairs);
        ++result.appliedDifferingClusterCount;
    }

    std::map<int, std::vector<int>> grouped;
    for (std::size_t index = 0; index < branches.size(); ++index) {
        if (excluded[index]) {
            result.excludedInternalBranchIndices.push_back(
                static_cast<int>(index));
            continue;
        }
        grouped[roots.find(static_cast<int>(index))].push_back(
            static_cast<int>(index));
    }
    result.rootBranchIndices.reserve(grouped.size());
    for (auto& item : grouped) {
        result.rootBranchIndices.push_back(std::move(item.second));
    }
    return result;
}

BranchDirection estimateMultiScaleBranchDirection(
    const Grid2D& grid,
    const std::vector<SkeletonPoint>& pointsFromJunction,
    const std::vector<int>& windows) {
    BranchDirection result;
    if (pointsFromJunction.size() < 2) {
        return result;
    }
    double sumX = 0.0;
    double sumY = 0.0;
    int usedWindows = 0;
    for (const int requestedWindow : windows) {
        if (requestedWindow < 1) {
            continue;
        }
        const int window = std::min(
            requestedWindow,
            static_cast<int>(pointsFromJunction.size()) - 1);
        const SkeletonPoint& origin = pointsFromJunction.front();
        const SkeletonPoint& sample =
            pointsFromJunction[static_cast<std::size_t>(window)];
        double directionX =
            static_cast<double>(sample.x - origin.x) * grid.dx();
        double directionY =
            static_cast<double>(sample.y - origin.y) * grid.dy();
        const double norm =
            vectorNorm(directionX, directionY);
        if (norm <= kEpsilon) {
            continue;
        }
        directionX /= norm;
        directionY /= norm;
        sumX += directionX;
        sumY += directionY;
        ++usedWindows;
    }
    const double norm = vectorNorm(sumX, sumY);
    if (usedWindows <= 0 || norm <= kEpsilon) {
        return result;
    }
    result.x = sumX / norm;
    result.y = sumY / norm;
    result.valid = true;
    return result;
}

JunctionAttributionResult matchBranchEnds(
    const std::vector<BranchAttributionEnd>& rawEnds,
    const BranchAttributionOptions& options) {
    constexpr int kMaximumExactEnds = 12;
    const bool finiteWeights =
        std::isfinite(options.angleWeight) &&
        std::isfinite(options.attributeWeight) &&
        std::isfinite(options.widthWeight) &&
        std::isfinite(options.backgroundWeight);
    const bool nonNegativeWeights =
        options.angleWeight >= 0.0 &&
        options.attributeWeight >= 0.0 &&
        options.widthWeight >= 0.0 &&
        options.backgroundWeight >= 0.0;
    const double weightSum =
        options.angleWeight +
        options.attributeWeight +
        options.widthWeight +
        options.backgroundWeight;
    if (options.maxExactEnds < 2 ||
        options.maxExactEnds > kMaximumExactEnds ||
        !std::isfinite(options.maxTurnAngleDegrees) ||
        options.maxTurnAngleDegrees < 0.0 ||
        options.maxTurnAngleDegrees > 180.0 ||
        !std::isfinite(options.maxPairCost) ||
        options.maxPairCost < 0.0 ||
        !std::isfinite(options.unmatchedCost) ||
        options.unmatchedCost < 0.0 ||
        !std::isfinite(options.ambiguityMargin) ||
        options.ambiguityMargin < 0.0 ||
        !finiteWeights ||
        !nonNegativeWeights ||
        !std::isfinite(weightSum) ||
        weightSum <= kEpsilon) {
        throw std::runtime_error(
            "Invalid branch attribution matching options.");
    }
    std::vector<BranchAttributionEnd> ends = rawEnds;
    std::sort(
        ends.begin(),
        ends.end(),
        [](const BranchAttributionEnd& first,
           const BranchAttributionEnd& second) {
            return first.branchId < second.branchId;
        });
    for (std::size_t index = 1; index < ends.size(); ++index) {
        if (ends[index - 1].branchId == ends[index].branchId) {
            throw std::runtime_error(
                "Branch attribution ends require unique branch ids per junction.");
        }
    }

    std::vector<std::vector<PairEvidence>> pairEvidence(
        ends.size(),
        std::vector<PairEvidence>(ends.size()));
    for (std::size_t first = 0; first < ends.size(); ++first) {
        for (std::size_t second = first + 1;
             second < ends.size();
             ++second) {
            pairEvidence[first][second] =
                evaluatePair(ends[first], ends[second], options);
            pairEvidence[second][first] =
                pairEvidence[first][second];
        }
    }

    MatchingSolution best;
    MatchingSolution alternative;
    bool usedExact = static_cast<int>(ends.size()) <=
        options.maxExactEnds;
    if (usedExact) {
        const std::vector<MatchingSolution> solutions =
            enumerateMatchings(ends, pairEvidence, options);
        if (!solutions.empty()) {
            best = solutions.front();
        }
        if (solutions.size() > 1) {
            alternative = solutions[1];
        } else {
            alternative.cost =
                std::numeric_limits<double>::infinity();
        }
    } else {
        best = greedyMatching(ends, pairEvidence, options);
        alternative.cost =
            std::numeric_limits<double>::infinity();
    }

    JunctionAttributionResult result;
    result.bestCost = best.cost;
    result.alternativeCost = alternative.cost;
    result.usedExactMatching = usedExact;
    result.unmatchedBranchIds = best.unmatched;
    for (const auto& pair : best.pairs) {
        const auto firstIt = std::lower_bound(
            ends.begin(),
            ends.end(),
            pair.first,
            [](const BranchAttributionEnd& end, int branchId) {
                return end.branchId < branchId;
            });
        const auto secondIt = std::lower_bound(
            ends.begin(),
            ends.end(),
            pair.second,
            [](const BranchAttributionEnd& end, int branchId) {
                return end.branchId < branchId;
            });
        const std::size_t firstIndex =
            static_cast<std::size_t>(firstIt - ends.begin());
        const std::size_t secondIndex =
            static_cast<std::size_t>(secondIt - ends.begin());
        const PairEvidence& evidence =
            pairEvidence[firstIndex][secondIndex];
        result.pairs.push_back({
            pair.first,
            pair.second,
            evidence.cost,
            evidence.turnAngleDegrees,
            evidence.angleCost,
            evidence.attributeCost,
            evidence.widthCost,
            evidence.backgroundCost});
    }
    if (std::isfinite(result.alternativeCost)) {
        result.confidence = std::max(
            0.0,
            result.alternativeCost - result.bestCost);
        result.ambiguous =
            result.confidence <= options.ambiguityMargin;
    }
    return result;
}

JunctionAttributionResult matchBranchEndsLegacyGreedy(
    const std::vector<BranchAttributionEnd>& ends,
    const BranchAttributionOptions& options) {
    JunctionAttributionResult result;
    result.usedExactMatching = false;
    std::vector<unsigned char> used(ends.size(), 0);
    for (std::size_t first = 0; first < ends.size(); ++first) {
        if (used[first]) {
            continue;
        }
        int bestSecond = -1;
        double bestTurnAngle = options.maxTurnAngleDegrees;
        for (std::size_t second = first + 1;
             second < ends.size();
             ++second) {
            if (used[second] ||
                ends[first].branchId == ends[second].branchId) {
                continue;
            }
            const double turnAngle = angleDegrees(
                ends[first].directionX,
                ends[first].directionY,
                -ends[second].directionX,
                -ends[second].directionY);
            if (turnAngle <= bestTurnAngle) {
                bestTurnAngle = turnAngle;
                bestSecond = static_cast<int>(second);
            }
        }
        if (bestSecond < 0) {
            used[first] = 1;
            result.unmatchedBranchIds.push_back(
                ends[first].branchId);
            continue;
        }
        const std::size_t second =
            static_cast<std::size_t>(bestSecond);
        used[first] = 1;
        used[second] = 1;
        result.bestCost += bestTurnAngle / 180.0;
        result.pairs.push_back({
            ends[first].branchId,
            ends[second].branchId,
            bestTurnAngle / 180.0,
            bestTurnAngle,
            bestTurnAngle / 180.0,
            0.0,
            0.0,
            0.0});
    }
    result.unmatchedBranchIds =
        normalizedIds(std::move(result.unmatchedBranchIds));
    return result;
}

BranchAttributionSummary analyzeBranchAttribution(
    const Grid2D& grid,
    const std::vector<const SkeletonBranch*>& branches,
    const BranchAttributionOptions& options) {
    if (options.junctionClusterRadius < 0 ||
        options.attributeSampleCells < 1 ||
        options.widthSampleRadius < 1) {
        throw std::runtime_error(
            "Invalid branch attribution analysis options.");
    }
    std::vector<RawBranchEnd> rawEnds;
    for (const SkeletonBranch* branch : branches) {
        if (!branch || branch->points.size() < 2) {
            continue;
        }
        if (branch->startIsJunction) {
            rawEnds.push_back({
                branch,
                branch->points.front(),
                branch->points});
        }
        if (branch->endIsJunction) {
            std::vector<SkeletonPoint> reversed(
                branch->points.rbegin(),
                branch->points.rend());
            rawEnds.push_back({
                branch,
                branch->points.back(),
                std::move(reversed)});
        }
    }

    BranchAttributionSummary summary;
    if (rawEnds.empty()) {
        return summary;
    }
    DisjointSet clusters(static_cast<int>(rawEnds.size()));
    for (std::size_t first = 0; first < rawEnds.size(); ++first) {
        for (std::size_t second = first + 1;
             second < rawEnds.size();
             ++second) {
            const int dx = std::abs(
                rawEnds[first].junction.x -
                rawEnds[second].junction.x);
            const int dy = std::abs(
                rawEnds[first].junction.y -
                rawEnds[second].junction.y);
            if (std::max(dx, dy) <=
                options.junctionClusterRadius) {
                clusters.unite(
                    static_cast<int>(first),
                    static_cast<int>(second));
            }
        }
    }

    std::vector<std::vector<std::size_t>> grouped;
    std::vector<int> groupByRoot(rawEnds.size(), -1);
    for (std::size_t index = 0; index < rawEnds.size(); ++index) {
        const int root = clusters.find(static_cast<int>(index));
        int groupIndex = groupByRoot[static_cast<std::size_t>(root)];
        if (groupIndex < 0) {
            groupIndex = static_cast<int>(grouped.size());
            groupByRoot[static_cast<std::size_t>(root)] = groupIndex;
            grouped.emplace_back();
        }
        grouped[static_cast<std::size_t>(groupIndex)].push_back(index);
    }

    int nextClusterId = 1;
    for (const auto& group : grouped) {
        if (group.size() < 2) {
            continue;
        }
        std::vector<BranchAttributionEnd> ends;
        ends.reserve(group.size());
        for (const std::size_t index : group) {
            const int branchId =
                rawEnds[index].branch
                ? rawEnds[index].branch->id
                : -1;
            const int occurrences = static_cast<int>(std::count_if(
                group.begin(),
                group.end(),
                [&](std::size_t otherIndex) {
                    return rawEnds[otherIndex].branch &&
                        rawEnds[otherIndex].branch->id == branchId;
                }));
            if (occurrences > 1) {
                continue;
            }
            ends.push_back(
                buildEndEvidence(grid, rawEnds[index], options));
        }
        if (ends.size() < 2) {
            continue;
        }
        JunctionAttributionResult junction =
            matchBranchEnds(ends, options);
        junction.clusterId = nextClusterId++;
        junction.minGridX = rawEnds[group.front()].junction.x;
        junction.maxGridX = junction.minGridX;
        junction.minGridY = rawEnds[group.front()].junction.y;
        junction.maxGridY = junction.minGridY;
        double centerGridX = 0.0;
        double centerGridY = 0.0;
        for (const std::size_t index : group) {
            const SkeletonPoint& point = rawEnds[index].junction;
            junction.minGridX = std::min(junction.minGridX, point.x);
            junction.maxGridX = std::max(junction.maxGridX, point.x);
            junction.minGridY = std::min(junction.minGridY, point.y);
            junction.maxGridY = std::max(junction.maxGridY, point.y);
            centerGridX += static_cast<double>(point.x);
            centerGridY += static_cast<double>(point.y);
        }
        const double groupSize = static_cast<double>(group.size());
        junction.centerGridX = centerGridX / groupSize;
        junction.centerGridY = centerGridY / groupSize;
        junction.centerWorldX =
            grid.worldX(junction.centerGridX + 0.5);
        junction.centerWorldY =
            grid.worldY(junction.centerGridY + 0.5);
        std::set<std::pair<int, int>> uniqueJunctionPoints;
        for (const std::size_t index : group) {
            const SkeletonPoint& point = rawEnds[index].junction;
            uniqueJunctionPoints.insert({point.x, point.y});
        }
        junction.junctionPoints.reserve(
            uniqueJunctionPoints.size());
        for (const auto& point : uniqueJunctionPoints) {
            junction.junctionPoints.push_back(
                {point.first, point.second});
        }
        appendLegacyGreedyEvidence(
            rawEnds,
            group,
            options.maxTurnAngleDegrees,
            junction);
        junction.pairingDiffersFromLegacy =
            normalizedPairIds(junction.pairs) !=
            normalizedPairIds(junction.legacyPairs);
        std::vector<int> externalBranchIds;
        externalBranchIds.reserve(ends.size());
        for (const BranchAttributionEnd& end : ends) {
            externalBranchIds.push_back(end.branchId);
        }
        junction.differsFromLegacy =
            !hasSameExternalConnectivity(
                externalBranchIds,
                junction.legacyPairs,
                junction.pairs);
        junction.acceptedPairingDiffersFromLegacy =
            junction.usedExactMatching &&
            !junction.ambiguous &&
            junction.pairingDiffersFromLegacy;
        junction.acceptedDiffersFromLegacy =
            junction.usedExactMatching &&
            !junction.ambiguous &&
            junction.differsFromLegacy;
        ++summary.junctionClusterCount;
        if (junction.ambiguous) {
            ++summary.ambiguousClusterCount;
        } else {
            summary.acceptedPairCount +=
                static_cast<int>(junction.pairs.size());
        }
        summary.unmatchedEndCount +=
            static_cast<int>(junction.unmatchedBranchIds.size());
        if (!junction.usedExactMatching) {
            ++summary.greedyFallbackClusterCount;
        }
        if (junction.pairingDiffersFromLegacy) {
            ++summary.candidatePairingDifferingClusterCount;
            ++summary.pairingDifferingClusterCount;
        }
        if (junction.differsFromLegacy) {
            ++summary.candidateDifferingClusterCount;
            ++summary.differingClusterCount;
        }
        if (junction.acceptedPairingDiffersFromLegacy) {
            ++summary.acceptedPairingDifferingClusterCount;
        }
        if (junction.acceptedDiffersFromLegacy) {
            ++summary.acceptedDifferingClusterCount;
        }
        summary.junctions.push_back(std::move(junction));
    }
    return summary;
}

} // namespace fpt
