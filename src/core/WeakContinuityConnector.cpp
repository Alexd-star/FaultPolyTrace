#include "WeakContinuityConnector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace fpt {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct PairSeed {
    int endpointA = -1;
    int endpointB = -1;
    double distance = 0.0;
    double angleA = 180.0;
    double angleB = 180.0;
    double widthA = 0.0;
    double widthB = 0.0;
    double geometricScore = 0.0;
};

struct SearchState {
    double priority = 0.0;
    double cost = 0.0;
    int state = -1;
    bool operator>(const SearchState& other) const {
        if (priority != other.priority) {
            return priority > other.priority;
        }
        return state > other.state;
    }
};

double clamp01(double value) {
    return std::max(0.0, std::min(1.0, value));
}

double length(Vec2 value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec2 normalized(Vec2 value) {
    const double n = length(value);
    if (n <= 1e-12) {
        return {};
    }
    return {value.x / n, value.y / n};
}

double dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

double distance(const SkeletonPoint& a, const SkeletonPoint& b) {
    return std::hypot(static_cast<double>(b.x - a.x),
                      static_cast<double>(b.y - a.y));
}

double angleDegrees(Vec2 a, Vec2 b) {
    const double denominator = length(a) * length(b);
    if (denominator <= 1e-12) {
        return 180.0;
    }
    const double cosine = std::max(-1.0, std::min(1.0, dot(a, b) / denominator));
    return std::acos(cosine) * 180.0 / kPi;
}

double pointSegmentDistance(double x, double y,
                            const SkeletonPoint& a,
                            const SkeletonPoint& b) {
    const double vx = static_cast<double>(b.x - a.x);
    const double vy = static_cast<double>(b.y - a.y);
    const double denominator = vx * vx + vy * vy;
    if (denominator <= 1e-12) {
        return std::hypot(x - a.x, y - a.y);
    }
    const double t = std::max(0.0, std::min(1.0,
        ((x - a.x) * vx + (y - a.y) * vy) / denominator));
    return std::hypot(x - (a.x + t * vx), y - (a.y + t * vy));
}

std::map<std::pair<int, int>, Vec2> endpointTangents(
    const SkeletonGraph& graph,
    int tangentWindow) {
    std::map<std::pair<int, int>, Vec2> tangents;
    for (const SkeletonBranch& branch : graph.branches) {
        if (branch.points.size() < 2) {
            continue;
        }
        const std::size_t reach = std::min<std::size_t>(
            static_cast<std::size_t>(std::max(1, tangentWindow)),
            branch.points.size() - 1);
        const SkeletonPoint& front = branch.points.front();
        const SkeletonPoint& frontInside = branch.points[reach];
        tangents[{front.x, front.y}] = normalized({
            static_cast<double>(front.x - frontInside.x),
            static_cast<double>(front.y - frontInside.y)});

        const SkeletonPoint& back = branch.points.back();
        const SkeletonPoint& backInside = branch.points[branch.points.size() - 1 - reach];
        tangents[{back.x, back.y}] = normalized({
            static_cast<double>(back.x - backInside.x),
            static_cast<double>(back.y - backInside.y)});
    }
    return tangents;
}

std::vector<int> connectedComponentLabels(const BinaryMask& mask) {
    const int width = mask.width();
    const int height = mask.height();
    std::vector<int> labels(static_cast<std::size_t>(width * height), -1);
    std::queue<SkeletonPoint> queue;
    int nextLabel = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            if (!mask.get(x, y) || labels[static_cast<std::size_t>(index)] >= 0) {
                continue;
            }
            labels[static_cast<std::size_t>(index)] = nextLabel;
            queue.push({x, y});
            while (!queue.empty()) {
                const SkeletonPoint point = queue.front();
                queue.pop();
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) {
                            continue;
                        }
                        const int nx = point.x + dx;
                        const int ny = point.y + dy;
                        if (!mask.inBounds(nx, ny) || !mask.get(nx, ny)) {
                            continue;
                        }
                        const int neighborIndex = ny * width + nx;
                        if (labels[static_cast<std::size_t>(neighborIndex)] >= 0) {
                            continue;
                        }
                        labels[static_cast<std::size_t>(neighborIndex)] = nextLabel;
                        queue.push({nx, ny});
                    }
                }
            }
            ++nextLabel;
        }
    }
    return labels;
}

double estimateHalfWidth(const BinaryMask& mask, const SkeletonPoint& point) {
    constexpr int kMaximumProbeRadius = 12;
    for (int radius = 1; radius <= kMaximumProbeRadius; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != radius) {
                    continue;
                }
                const int x = point.x + dx;
                const int y = point.y + dy;
                if (!mask.inBounds(x, y) || !mask.get(x, y)) {
                    return static_cast<double>(radius);
                }
            }
        }
    }
    return static_cast<double>(kMaximumProbeRadius + 1);
}

double percentile(std::vector<double> values, double fraction) {
    if (values.empty()) {
        return 0.0;
    }
    const std::size_t index = static_cast<std::size_t>(
        std::floor(clamp01(fraction) * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

double localBackgroundValue(const Grid2D& grid,
                            const SkeletonPoint& a,
                            const SkeletonPoint& b,
                            int corridorRadius) {
    std::vector<double> values;
    const int padding = std::max(2, corridorRadius);
    const int minX = std::max(0, std::min(a.x, b.x) - padding);
    const int maxX = std::min(grid.nx() - 1, std::max(a.x, b.x) + padding);
    const int minY = std::max(0, std::min(a.y, b.y) - padding);
    const int maxY = std::min(grid.ny() - 1, std::max(a.y, b.y) + padding);
    values.reserve(static_cast<std::size_t>((maxX - minX + 1) * (maxY - minY + 1)));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const double distanceToAxis = pointSegmentDistance(x, y, a, b);
            if (distanceToAxis >= corridorRadius * 0.65 && distanceToAxis <= corridorRadius) {
                values.push_back(grid.value(x, y));
            }
        }
    }
    return percentile(std::move(values), 0.35);
}

std::vector<SkeletonPoint> findPath(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    const SkeletonPoint& start,
    const SkeletonPoint& target,
    double supportThreshold,
    double background,
    const WeakContinuityOptions& options,
    double* totalCost) {
    static const std::array<int, 8> dx{{1, 1, 0, -1, -1, -1, 0, 1}};
    static const std::array<int, 8> dy{{0, 1, 1, 1, 0, -1, -1, -1}};
    constexpr int kNoDirection = 8;
    constexpr int kDirectionCount = 9;

    const int padding = options.corridorRadius + 2;
    const int minX = std::max(0, std::min(start.x, target.x) - padding);
    const int maxX = std::min(grid.nx() - 1, std::max(start.x, target.x) + padding);
    const int minY = std::max(0, std::min(start.y, target.y) - padding);
    const int maxY = std::min(grid.ny() - 1, std::max(start.y, target.y) + padding);
    const int width = maxX - minX + 1;
    const int height = maxY - minY + 1;
    const int stateCount = width * height * kDirectionCount;
    std::vector<double> costs(static_cast<std::size_t>(stateCount), kInfinity);
    std::vector<int> previous(static_cast<std::size_t>(stateCount), -1);

    auto encode = [&](int x, int y, int direction) {
        return (((y - minY) * width + (x - minX)) * kDirectionCount) + direction;
    };
    auto decode = [&](int state, int* x, int* y, int* direction) {
        *direction = state % kDirectionCount;
        const int pixel = state / kDirectionCount;
        *x = minX + pixel % width;
        *y = minY + pixel / width;
    };

    const Vec2 globalDirection = normalized({
        static_cast<double>(target.x - start.x),
        static_cast<double>(target.y - start.y)});
    std::priority_queue<SearchState, std::vector<SearchState>, std::greater<SearchState>> queue;
    const int startState = encode(start.x, start.y, kNoDirection);
    costs[static_cast<std::size_t>(startState)] = 0.0;
    queue.push({distance(start, target), 0.0, startState});
    int targetState = -1;

    auto nearThirdPartySkeleton = [&](int x, int y) {
        const int radius = options.thirdPartySkeletonClearanceRadius;
        for (int oy = -radius; oy <= radius; ++oy) {
            for (int ox = -radius; ox <= radius; ++ox) {
                if (ox * ox + oy * oy > radius * radius) {
                    continue;
                }
                if (skeleton.get(x + ox, y + oy)) {
                    return true;
                }
            }
        }
        return false;
    };

    while (!queue.empty()) {
        const SearchState current = queue.top();
        queue.pop();
        if (current.cost != costs[static_cast<std::size_t>(current.state)]) {
            continue;
        }
        int x = 0;
        int y = 0;
        int previousDirection = kNoDirection;
        decode(current.state, &x, &y, &previousDirection);
        if (x == target.x && y == target.y) {
            targetState = current.state;
            break;
        }

        for (int direction = 0; direction < 8; ++direction) {
            const int nx = x + dx[static_cast<std::size_t>(direction)];
            const int ny = y + dy[static_cast<std::size_t>(direction)];
            if (nx < minX || nx > maxX || ny < minY || ny > maxY) {
                continue;
            }
            if (pointSegmentDistance(nx, ny, start, target) > options.corridorRadius) {
                continue;
            }
            const double startDistance = std::hypot(nx - start.x, ny - start.y);
            const double targetDistance = std::hypot(nx - target.x, ny - target.y);
            if (nearThirdPartySkeleton(nx, ny) &&
                startDistance > options.endpointExclusionRadius &&
                targetDistance > options.endpointExclusionRadius) {
                continue;
            }

            const Vec2 movement = normalized({static_cast<double>(dx[direction]),
                                              static_cast<double>(dy[direction])});
            const double progress = dot(movement, globalDirection);
            if (progress < -0.20) {
                continue;
            }
            const double stepLength = (dx[direction] != 0 && dy[direction] != 0)
                ? std::sqrt(2.0) : 1.0;
            const double denominator = std::max(1e-9, supportThreshold - background);
            const double support = clamp01((grid.value(nx, ny) - background) / denominator);
            const double attributePenalty = options.attributeWeight * (1.0 - support) * (1.0 - support);
            const double directionPenalty = options.directionWeight * (1.0 - std::max(0.0, progress));
            double turnPenalty = 0.0;
            if (previousDirection != kNoDirection) {
                const Vec2 previousMovement = normalized({
                    static_cast<double>(dx[static_cast<std::size_t>(previousDirection)]),
                    static_cast<double>(dy[static_cast<std::size_t>(previousDirection)])});
                turnPenalty = options.curvatureWeight *
                    angleDegrees(previousMovement, movement) / 45.0;
            }
            const double nextCost = current.cost + stepLength * (1.0 + attributePenalty) +
                directionPenalty + turnPenalty;
            const int nextState = encode(nx, ny, direction);
            if (nextCost + 1e-12 >= costs[static_cast<std::size_t>(nextState)]) {
                continue;
            }
            costs[static_cast<std::size_t>(nextState)] = nextCost;
            previous[static_cast<std::size_t>(nextState)] = current.state;
            const double heuristic = std::hypot(target.x - nx, target.y - ny);
            queue.push({nextCost + heuristic, nextCost, nextState});
        }
    }

    if (targetState < 0) {
        return {};
    }
    std::vector<SkeletonPoint> reversed;
    for (int state = targetState; state >= 0; state = previous[static_cast<std::size_t>(state)]) {
        int x = 0;
        int y = 0;
        int direction = 0;
        decode(state, &x, &y, &direction);
        if (reversed.empty() || reversed.back().x != x || reversed.back().y != y) {
            reversed.push_back({x, y});
        }
    }
    std::reverse(reversed.begin(), reversed.end());
    if (totalCost) {
        *totalCost = costs[static_cast<std::size_t>(targetState)];
    }
    return reversed;
}

void measurePath(WeakContinuityCandidate& candidate,
                 const Grid2D& grid,
                 const BinaryMask& skeleton,
                 double supportThreshold,
                 double background,
                 const WeakContinuityOptions& options) {
    if (candidate.path.empty()) {
        return;
    }
    candidate.localBackground = background;
    candidate.minimumSupportedValue = std::max(
        supportThreshold * options.minAttributeRatio,
        background + options.localContrastRatio * std::max(0.0, supportThreshold - background));
    int supported = 0;
    int unsupportedRun = 0;
    double supportSum = 0.0;
    const double supportDenominator = std::max(1e-9, supportThreshold - background);
    for (std::size_t index = 0; index < candidate.path.size(); ++index) {
        const SkeletonPoint& point = candidate.path[index];
        const double value = grid.value(point.x, point.y);
        supportSum += clamp01((value - background) / supportDenominator);
        if (value >= candidate.minimumSupportedValue) {
            ++supported;
            unsupportedRun = 0;
        } else {
            ++unsupportedRun;
            candidate.maxUnsupportedRun = std::max(candidate.maxUnsupportedRun, unsupportedRun);
        }
        if (!skeleton.get(point.x, point.y)) {
            ++candidate.addedPathPixels;
        }
        if (index > 0) {
            candidate.pathLength += distance(candidate.path[index - 1], point);
        }
    }
    candidate.supportRatio = static_cast<double>(supported) /
        static_cast<double>(candidate.path.size());
    candidate.meanSupport = supportSum / static_cast<double>(candidate.path.size());
    candidate.pathLengthRatio = candidate.endpointDistance > 1e-9
        ? candidate.pathLength / candidate.endpointDistance : 1.0;

    constexpr std::size_t kTurnWindow = 3;
    if (candidate.path.size() > kTurnWindow * 2) {
        for (std::size_t index = kTurnWindow;
             index + kTurnWindow < candidate.path.size(); ++index) {
            const SkeletonPoint& before = candidate.path[index - kTurnWindow];
            const SkeletonPoint& center = candidate.path[index];
            const SkeletonPoint& after = candidate.path[index + kTurnWindow];
            const Vec2 incoming{static_cast<double>(center.x - before.x),
                                static_cast<double>(center.y - before.y)};
            const Vec2 outgoing{static_cast<double>(after.x - center.x),
                                static_cast<double>(after.y - center.y)};
            candidate.maxTurnAngleDegrees = std::max(
                candidate.maxTurnAngleDegrees, angleDegrees(incoming, outgoing));
        }
    }
}

} // namespace

void validateWeakContinuityOptions(const WeakContinuityOptions& options) {
    if (options.maxDistance < 1 || options.tangentWindow < 1 ||
        options.corridorRadius < 1 || options.maxCandidatesPerEndpoint < 1) {
        throw std::runtime_error("Weak continuity distance/window/corridor/candidate limits must be positive.");
    }
    if (options.maxEndpointAngleDegrees < 0.0 || options.maxEndpointAngleDegrees > 90.0 ||
        options.maxTurnAngleDegrees < 0.0 || options.maxTurnAngleDegrees > 180.0) {
        throw std::runtime_error("Weak continuity angle limits are out of range.");
    }
    if (options.maxWidthRatio < 1.0 || options.maxPathLengthRatio < 1.0) {
        throw std::runtime_error("Weak continuity width/path ratios must be at least one.");
    }
    if (options.minAttributeRatio < 0.0 || options.minAttributeRatio > 1.0 ||
        options.localContrastRatio < 0.0 || options.localContrastRatio > 1.0 ||
        options.minSupportRatio < 0.0 || options.minSupportRatio > 1.0 ||
        options.minMeanSupport < 0.0 || options.minMeanSupport > 1.0) {
        throw std::runtime_error("Weak continuity support ratios must be between zero and one.");
    }
    if (options.maxUnsupportedRun < 0 || options.endpointExclusionRadius < 0 ||
        options.thirdPartySkeletonClearanceRadius < 0 ||
        options.minCompetitionMargin < 0.0 || options.attributeWeight < 0.0 ||
        options.directionWeight < 0.0 || options.curvatureWeight < 0.0 ||
        options.maxEvidenceRecords < 1) {
        throw std::runtime_error("Weak continuity penalties and limits must be non-negative.");
    }
}

WeakContinuityResult connectWeakContinuityPaths(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    const BinaryMask& attributeMask,
    const SkeletonGraph& graph,
    double supportThreshold,
    const WeakContinuityOptions& options,
    const BinaryMask* componentConnectivityMask) {
    validateWeakContinuityOptions(options);
    WeakContinuityResult result{BinaryMask(skeleton), {}, {}};
    result.stats.endpointCount = static_cast<int>(graph.endpoints.size());
    if (graph.endpoints.size() < 2) {
        return result;
    }

    if (componentConnectivityMask &&
        (componentConnectivityMask->width() != skeleton.width() ||
         componentConnectivityMask->height() != skeleton.height())) {
        throw std::runtime_error(
            "Weak continuity component connectivity mask must match the skeleton.");
    }
    const auto tangents = endpointTangents(graph, options.tangentWindow);
    const std::vector<int> labels = connectedComponentLabels(
        componentConnectivityMask ? *componentConnectivityMask : skeleton);
    std::vector<PairSeed> seeds;
    std::vector<std::vector<PairSeed>> byEndpoint(graph.endpoints.size());

    for (std::size_t i = 0; i < graph.endpoints.size(); ++i) {
        const SkeletonPoint& a = graph.endpoints[i];
        const auto tangentAIt = tangents.find({a.x, a.y});
        if (tangentAIt == tangents.end()) {
            continue;
        }
        for (std::size_t j = i + 1; j < graph.endpoints.size(); ++j) {
            const SkeletonPoint& b = graph.endpoints[j];
            const double endpointDistance = distance(a, b);
            if (endpointDistance > options.maxDistance) {
                continue;
            }
            ++result.stats.distanceCandidatePairs;
            const int labelA = labels[static_cast<std::size_t>(a.y * skeleton.width() + a.x)];
            const int labelB = labels[static_cast<std::size_t>(b.y * skeleton.width() + b.x)];
            if (labelA >= 0 && labelA == labelB) {
                ++result.stats.sameComponentRejectedPairs;
                continue;
            }
            const auto tangentBIt = tangents.find({b.x, b.y});
            if (tangentBIt == tangents.end()) {
                continue;
            }
            const Vec2 gapAB{static_cast<double>(b.x - a.x), static_cast<double>(b.y - a.y)};
            const Vec2 gapBA{-gapAB.x, -gapAB.y};
            const double angleA = angleDegrees(tangentAIt->second, gapAB);
            const double angleB = angleDegrees(tangentBIt->second, gapBA);
            if (angleA > options.maxEndpointAngleDegrees ||
                angleB > options.maxEndpointAngleDegrees) {
                ++result.stats.directionRejectedPairs;
                continue;
            }
            const double widthA = estimateHalfWidth(attributeMask, a);
            const double widthB = estimateHalfWidth(attributeMask, b);
            const double widthRatio = std::max(widthA, widthB) /
                std::max(1.0, std::min(widthA, widthB));
            if (widthRatio > options.maxWidthRatio) {
                ++result.stats.widthRejectedPairs;
                continue;
            }
            PairSeed seed;
            seed.endpointA = static_cast<int>(i);
            seed.endpointB = static_cast<int>(j);
            seed.distance = endpointDistance;
            seed.angleA = angleA;
            seed.angleB = angleB;
            seed.widthA = widthA;
            seed.widthB = widthB;
            seed.geometricScore = endpointDistance / options.maxDistance +
                (angleA + angleB) / (2.0 * options.maxEndpointAngleDegrees);
            byEndpoint[i].push_back(seed);
            byEndpoint[j].push_back(seed);
        }
    }

    std::set<std::pair<int, int>> selectedPairs;
    for (std::vector<PairSeed>& endpointSeeds : byEndpoint) {
        std::sort(endpointSeeds.begin(), endpointSeeds.end(), [](const PairSeed& lhs, const PairSeed& rhs) {
            return std::tie(lhs.geometricScore, lhs.endpointA, lhs.endpointB) <
                   std::tie(rhs.geometricScore, rhs.endpointA, rhs.endpointB);
        });
        const std::size_t count = std::min<std::size_t>(
            endpointSeeds.size(), static_cast<std::size_t>(options.maxCandidatesPerEndpoint));
        for (std::size_t index = 0; index < count; ++index) {
            selectedPairs.insert({endpointSeeds[index].endpointA, endpointSeeds[index].endpointB});
        }
    }
    for (const std::vector<PairSeed>& endpointSeeds : byEndpoint) {
        for (const PairSeed& seed : endpointSeeds) {
            if (selectedPairs.count({seed.endpointA, seed.endpointB}) != 0) {
                seeds.push_back(seed);
            }
        }
    }
    std::sort(seeds.begin(), seeds.end(), [](const PairSeed& lhs, const PairSeed& rhs) {
        return std::tie(lhs.endpointA, lhs.endpointB) < std::tie(rhs.endpointA, rhs.endpointB);
    });
    seeds.erase(std::unique(seeds.begin(), seeds.end(), [](const PairSeed& lhs, const PairSeed& rhs) {
        return lhs.endpointA == rhs.endpointA && lhs.endpointB == rhs.endpointB;
    }), seeds.end());
    result.stats.preselectedPairs = static_cast<int>(seeds.size());

    std::vector<int> candidateEndpointA;
    std::vector<int> candidateEndpointB;
    for (const PairSeed& seed : seeds) {
        WeakContinuityCandidate candidate;
        candidate.id = static_cast<int>(result.candidates.size()) + 1;
        candidateEndpointA.push_back(seed.endpointA);
        candidateEndpointB.push_back(seed.endpointB);
        candidate.endpointA = graph.endpoints[static_cast<std::size_t>(seed.endpointA)];
        candidate.endpointB = graph.endpoints[static_cast<std::size_t>(seed.endpointB)];
        candidate.endpointDistance = seed.distance;
        candidate.angleA = seed.angleA;
        candidate.angleB = seed.angleB;
        candidate.widthA = seed.widthA;
        candidate.widthB = seed.widthB;
        const double background = localBackgroundValue(
            grid, candidate.endpointA, candidate.endpointB, options.corridorRadius);
        double totalCost = 0.0;
        ++result.stats.searchedPaths;
        candidate.path = findPath(
            grid, skeleton, candidate.endpointA, candidate.endpointB,
            supportThreshold, background, options, &totalCost);
        candidate.pathFound = !candidate.path.empty();
        if (!candidate.pathFound) {
            candidate.decision = "no_path_or_third_party_skeleton";
            ++result.stats.noPathRejected;
            result.candidates.push_back(std::move(candidate));
            continue;
        }
        ++result.stats.foundPaths;
        measurePath(candidate, grid, skeleton, supportThreshold, background, options);
        candidate.normalizedCost = totalCost / std::max(1.0, candidate.pathLength);
        if (candidate.supportRatio < options.minSupportRatio ||
            candidate.meanSupport < options.minMeanSupport) {
            candidate.decision = "insufficient_attribute_support";
            ++result.stats.attributeRejected;
        } else if (candidate.maxUnsupportedRun > options.maxUnsupportedRun) {
            candidate.decision = "unsupported_run_too_long";
            ++result.stats.unsupportedRunRejected;
        } else if (candidate.pathLengthRatio > options.maxPathLengthRatio) {
            candidate.decision = "path_detour_too_large";
            ++result.stats.detourRejected;
        } else if (candidate.maxTurnAngleDegrees > options.maxTurnAngleDegrees) {
            candidate.decision = "curvature_change_too_large";
            ++result.stats.curvatureRejected;
        } else {
            candidate.decision = "eligible";
        }
        result.candidates.push_back(std::move(candidate));
    }

    std::vector<std::vector<int>> eligibleByEndpoint(graph.endpoints.size());
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        if (result.candidates[index].decision != "eligible") {
            continue;
        }
        eligibleByEndpoint[static_cast<std::size_t>(candidateEndpointA[index])].push_back(static_cast<int>(index));
        eligibleByEndpoint[static_cast<std::size_t>(candidateEndpointB[index])].push_back(static_cast<int>(index));
    }
    for (std::vector<int>& indices : eligibleByEndpoint) {
        std::sort(indices.begin(), indices.end(), [&](int lhs, int rhs) {
            return std::tie(result.candidates[static_cast<std::size_t>(lhs)].normalizedCost, lhs) <
                   std::tie(result.candidates[static_cast<std::size_t>(rhs)].normalizedCost, rhs);
        });
    }

    std::vector<int> ranked;
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        if (result.candidates[index].decision == "eligible") {
            ranked.push_back(static_cast<int>(index));
        }
    }
    std::sort(ranked.begin(), ranked.end(), [&](int lhs, int rhs) {
        return std::tie(result.candidates[static_cast<std::size_t>(lhs)].normalizedCost, lhs) <
               std::tie(result.candidates[static_cast<std::size_t>(rhs)].normalizedCost, rhs);
    });

    std::vector<bool> endpointUsed(graph.endpoints.size(), false);
    for (int candidateIndex : ranked) {
        WeakContinuityCandidate& candidate = result.candidates[static_cast<std::size_t>(candidateIndex)];
        const int endpointA = candidateEndpointA[static_cast<std::size_t>(candidateIndex)];
        const int endpointB = candidateEndpointB[static_cast<std::size_t>(candidateIndex)];
        const auto margin = [&](int endpoint) {
            const std::vector<int>& alternatives = eligibleByEndpoint[static_cast<std::size_t>(endpoint)];
            if (alternatives.empty() || alternatives.front() != candidateIndex) {
                return -1.0;
            }
            if (alternatives.size() == 1) {
                return kInfinity;
            }
            return result.candidates[static_cast<std::size_t>(alternatives[1])].normalizedCost -
                   candidate.normalizedCost;
        };
        candidate.competitionMarginA = margin(endpointA);
        candidate.competitionMarginB = margin(endpointB);
        if (candidate.competitionMarginA < options.minCompetitionMargin ||
            candidate.competitionMarginB < options.minCompetitionMargin) {
            candidate.decision = "ambiguous_competing_endpoint";
            ++result.stats.competitionRejected;
            continue;
        }
        if (endpointUsed[static_cast<std::size_t>(endpointA)] ||
            endpointUsed[static_cast<std::size_t>(endpointB)]) {
            candidate.decision = "endpoint_already_used";
            ++result.stats.endpointReuseRejected;
            continue;
        }
        for (const SkeletonPoint& point : candidate.path) {
            result.mask.set(point.x, point.y, true);
        }
        endpointUsed[static_cast<std::size_t>(endpointA)] = true;
        endpointUsed[static_cast<std::size_t>(endpointB)] = true;
        candidate.accepted = true;
        candidate.decision = "accepted";
        ++result.stats.acceptedConnections;
        result.stats.addedPathPixels += candidate.addedPathPixels;
    }

    if (result.candidates.size() > static_cast<std::size_t>(options.maxEvidenceRecords)) {
        result.candidates.resize(static_cast<std::size_t>(options.maxEvidenceRecords));
    }
    return result;
}

} // namespace fpt
