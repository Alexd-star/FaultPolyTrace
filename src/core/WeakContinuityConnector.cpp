#include "WeakContinuityConnector.h"

#include "GridMetric.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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

struct BackgroundEstimate {
    double value = 0.0;
    int finiteSamples = 0;
    int invalidSamples = 0;
    bool valid = false;
};

double clamp01(double value) {
    if (!std::isfinite(value)) {
        return 0.0;
    }
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

Vec2 metricVector(
    const Grid2D& grid,
    const SkeletonPoint& from,
    const SkeletonPoint& to,
    bool physical) {
    return physical
        ? Vec2{static_cast<double>(to.x - from.x) * grid.dx(),
               static_cast<double>(to.y - from.y) * grid.dy()}
        : Vec2{static_cast<double>(to.x - from.x),
               static_cast<double>(to.y - from.y)};
}

double metricDistance(
    const Grid2D& grid,
    const SkeletonPoint& a,
    const SkeletonPoint& b,
    bool physical) {
    return length(metricVector(grid, a, b, physical));
}

double metricRadius(const Grid2D& grid, int cells, bool physical) {
    return physical ? static_cast<double>(cells) * std::max(grid.dx(), grid.dy())
                    : static_cast<double>(cells);
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

double pointSegmentDistanceMetric(
    const Grid2D& grid,
    double x,
    double y,
    const SkeletonPoint& a,
    const SkeletonPoint& b,
    bool physical) {
    if (!physical) {
        return pointSegmentDistance(x, y, a, b);
    }
    const double px = x * grid.dx();
    const double py = y * grid.dy();
    const double ax = static_cast<double>(a.x) * grid.dx();
    const double ay = static_cast<double>(a.y) * grid.dy();
    const double bx = static_cast<double>(b.x) * grid.dx();
    const double by = static_cast<double>(b.y) * grid.dy();
    const double vx = bx - ax;
    const double vy = by - ay;
    const double denominator = vx * vx + vy * vy;
    if (denominator <= 1e-12) {
        return std::hypot(px - ax, py - ay);
    }
    const double t = std::max(0.0, std::min(1.0,
        ((px - ax) * vx + (py - ay) * vy) / denominator));
    return std::hypot(px - (ax + t * vx), py - (ay + t * vy));
}

std::map<std::pair<int, int>, Vec2> endpointTangents(
    const Grid2D& grid,
    const SkeletonGraph& graph,
    int tangentWindow,
    bool physical) {
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
        tangents[{front.x, front.y}] = normalized(
            metricVector(grid, frontInside, front, physical));

        const SkeletonPoint& back = branch.points.back();
        const SkeletonPoint& backInside = branch.points[branch.points.size() - 1 - reach];
        tangents[{back.x, back.y}] = normalized(
            metricVector(grid, backInside, back, physical));
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
    const std::size_t index = static_cast<std::size_t>(
        std::floor(clamp01(fraction) * static_cast<double>(values.size() - 1)));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

BackgroundEstimate localBackgroundValue(const Grid2D& grid,
                                        const SkeletonPoint& a,
                                        const SkeletonPoint& b,
                                        int corridorRadius,
                                        bool physical) {
    BackgroundEstimate estimate;
    std::vector<double> values;
    const int padding = std::max(2, corridorRadius);
    const double radius = metricRadius(grid, corridorRadius, physical);
    const int minX = std::max(0, std::min(a.x, b.x) - padding);
    const int maxX = std::min(grid.nx() - 1, std::max(a.x, b.x) + padding);
    const int minY = std::max(0, std::min(a.y, b.y) - padding);
    const int maxY = std::min(grid.ny() - 1, std::max(a.y, b.y) + padding);
    values.reserve(static_cast<std::size_t>((maxX - minX + 1) * (maxY - minY + 1)));
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            const double distanceToAxis = pointSegmentDistanceMetric(
                grid, static_cast<double>(x), static_cast<double>(y), a, b, physical);
            if (distanceToAxis >= radius * 0.65 && distanceToAxis <= radius) {
                const double value = grid.value(x, y);
                if (std::isfinite(value)) {
                    values.push_back(value);
                } else {
                    ++estimate.invalidSamples;
                }
            }
        }
    }
    estimate.finiteSamples = static_cast<int>(values.size());
    if (!values.empty()) {
        estimate.value = percentile(std::move(values), 0.35);
        estimate.valid = std::isfinite(estimate.value);
    }
    return estimate;
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
    const double corridorRadius = metricRadius(
        grid, options.corridorRadius, options.usePhysicalMetric);
    const double endpointExclusionRadius = metricRadius(
        grid, options.endpointExclusionRadius, options.usePhysicalMetric);
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

    const Vec2 globalDirection = normalized(metricVector(
        grid, start, target, options.usePhysicalMetric));
    std::priority_queue<SearchState, std::vector<SearchState>, std::greater<SearchState>> queue;
    const int startState = encode(start.x, start.y, kNoDirection);
    costs[static_cast<std::size_t>(startState)] = 0.0;
    const double metricScale = options.usePhysicalMetric
        ? std::max(1e-9, std::min(grid.dx(), grid.dy())) : 1.0;
    queue.push({metricDistance(grid, start, target, options.usePhysicalMetric) /
                    metricScale,
                0.0, startState});
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
            if (pointSegmentDistanceMetric(
                    grid, static_cast<double>(nx), static_cast<double>(ny),
                    start, target, options.usePhysicalMetric) > corridorRadius) {
                continue;
            }
            const double startDistance = metricDistance(
                grid, start, {nx, ny}, options.usePhysicalMetric);
            const double targetDistance = metricDistance(
                grid, target, {nx, ny}, options.usePhysicalMetric);
            if (nearThirdPartySkeleton(nx, ny) &&
                startDistance > endpointExclusionRadius &&
                targetDistance > endpointExclusionRadius) {
                continue;
            }

            const Vec2 movement = normalized(metricVector(
                grid, {x, y}, {nx, ny}, options.usePhysicalMetric));
            const double progress = dot(movement, globalDirection);
            if (progress < -0.20) {
                continue;
            }
            const double stepLength = metricDistance(
                grid, {x, y}, {nx, ny}, options.usePhysicalMetric) / metricScale;
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
            const double heuristic = metricDistance(
                grid, {nx, ny}, target, options.usePhysicalMetric) / metricScale;
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
        if (index > 0) {
            candidate.pathLength += metricDistance(
                grid, candidate.path[index - 1], point, options.usePhysicalMetric);
        }
        if (!skeleton.get(point.x, point.y)) {
            ++candidate.addedPathPixels;
        } else if (options.excludeExistingSkeletonFromSupport) {
            ++candidate.excludedSkeletonSupportPixels;
            continue;
        }

        ++candidate.evaluatedSupportPixels;
        const double value = grid.value(point.x, point.y);
        if (!std::isfinite(value)) {
            ++candidate.invalidPathSamples;
        }
        supportSum += clamp01((value - background) / supportDenominator);
        if (std::isfinite(value) && value >= candidate.minimumSupportedValue) {
            ++supported;
            unsupportedRun = 0;
        } else {
            ++unsupportedRun;
            candidate.maxUnsupportedRun = std::max(candidate.maxUnsupportedRun, unsupportedRun);
        }
    }
    if (candidate.evaluatedSupportPixels > 0) {
        candidate.supportRatio = static_cast<double>(supported) /
            static_cast<double>(candidate.evaluatedSupportPixels);
        candidate.meanSupport = supportSum /
            static_cast<double>(candidate.evaluatedSupportPixels);
    }
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

long long orientationValue(
    const SkeletonPoint& a,
    const SkeletonPoint& b,
    const SkeletonPoint& c) {
    return static_cast<long long>(b.x - a.x) *
            static_cast<long long>(c.y - a.y) -
        static_cast<long long>(b.y - a.y) *
            static_cast<long long>(c.x - a.x);
}

bool samePoint(const SkeletonPoint& a, const SkeletonPoint& b) {
    return a.x == b.x && a.y == b.y;
}

bool onClosedSegment(
    const SkeletonPoint& a,
    const SkeletonPoint& b,
    const SkeletonPoint& point) {
    return orientationValue(a, b, point) == 0 &&
        point.x >= std::min(a.x, b.x) && point.x <= std::max(a.x, b.x) &&
        point.y >= std::min(a.y, b.y) && point.y <= std::max(a.y, b.y);
}

bool segmentsIntersect(
    const SkeletonPoint& a,
    const SkeletonPoint& b,
    const SkeletonPoint& c,
    const SkeletonPoint& d) {
    const long long o1 = orientationValue(a, b, c);
    const long long o2 = orientationValue(a, b, d);
    const long long o3 = orientationValue(c, d, a);
    const long long o4 = orientationValue(c, d, b);
    if (((o1 > 0 && o2 < 0) || (o1 < 0 && o2 > 0)) &&
        ((o3 > 0 && o4 < 0) || (o3 < 0 && o4 > 0))) {
        return true;
    }
    return (o1 == 0 && onClosedSegment(a, b, c)) ||
        (o2 == 0 && onClosedSegment(a, b, d)) ||
        (o3 == 0 && onClosedSegment(c, d, a)) ||
        (o4 == 0 && onClosedSegment(c, d, b));
}

bool fullPathEndpoint(
    const std::vector<SkeletonPoint>& path,
    const SkeletonPoint& point) {
    return !path.empty() &&
        (samePoint(path.front(), point) || samePoint(path.back(), point));
}

bool pathsSpatiallyConflict(
    const std::vector<SkeletonPoint>& first,
    const std::vector<SkeletonPoint>& second) {
    if (first.size() < 2 || second.size() < 2) {
        return false;
    }
    int firstMinX = first.front().x;
    int firstMaxX = first.front().x;
    int firstMinY = first.front().y;
    int firstMaxY = first.front().y;
    for (const SkeletonPoint& point : first) {
        firstMinX = std::min(firstMinX, point.x);
        firstMaxX = std::max(firstMaxX, point.x);
        firstMinY = std::min(firstMinY, point.y);
        firstMaxY = std::max(firstMaxY, point.y);
    }
    int secondMinX = second.front().x;
    int secondMaxX = second.front().x;
    int secondMinY = second.front().y;
    int secondMaxY = second.front().y;
    for (const SkeletonPoint& point : second) {
        secondMinX = std::min(secondMinX, point.x);
        secondMaxX = std::max(secondMaxX, point.x);
        secondMinY = std::min(secondMinY, point.y);
        secondMaxY = std::max(secondMaxY, point.y);
    }
    if (firstMaxX < secondMinX || secondMaxX < firstMinX ||
        firstMaxY < secondMinY || secondMaxY < firstMinY) {
        return false;
    }

    std::set<std::pair<int, int>> firstCells;
    for (const SkeletonPoint& point : first) {
        firstCells.insert({point.x, point.y});
    }
    for (const SkeletonPoint& point : second) {
        if (!firstCells.count({point.x, point.y})) {
            continue;
        }
        if (!(fullPathEndpoint(first, point) && fullPathEndpoint(second, point))) {
            return true;
        }
    }

    for (std::size_t firstIndex = 1; firstIndex < first.size(); ++firstIndex) {
        const SkeletonPoint& a = first[firstIndex - 1];
        const SkeletonPoint& b = first[firstIndex];
        for (std::size_t secondIndex = 1;
             secondIndex < second.size(); ++secondIndex) {
            const SkeletonPoint& c = second[secondIndex - 1];
            const SkeletonPoint& d = second[secondIndex];
            if (!segmentsIntersect(a, b, c, d)) {
                continue;
            }
            SkeletonPoint shared;
            bool sharedEndpoint = false;
            for (const SkeletonPoint* left : {&a, &b}) {
                for (const SkeletonPoint* right : {&c, &d}) {
                    if (samePoint(*left, *right)) {
                        shared = *left;
                        sharedEndpoint = true;
                    }
                }
            }
            if (sharedEndpoint && fullPathEndpoint(first, shared) &&
                fullPathEndpoint(second, shared)) {
                continue;
            }
            return true;
        }
    }
    return false;
}

void applyEndpointOnlyGlobalNoConnectMatching(
    WeakContinuityResult& result,
    const std::vector<int>& candidateEndpointA,
    const std::vector<int>& candidateEndpointB,
    const WeakContinuityOptions& options) {
    const std::size_t endpointCount =
        static_cast<std::size_t>(result.stats.endpointCount);
    std::vector<std::vector<int>> eligibleByEndpoint(endpointCount);
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        if (result.candidates[index].decision != "eligible") {
            continue;
        }
        eligibleByEndpoint[static_cast<std::size_t>(candidateEndpointA[index])].push_back(
            static_cast<int>(index));
        eligibleByEndpoint[static_cast<std::size_t>(candidateEndpointB[index])].push_back(
            static_cast<int>(index));
    }

    std::vector<unsigned char> selected(result.candidates.size(), 0);
    std::vector<unsigned char> visited(endpointCount, 0);
    for (std::size_t firstEndpoint = 0; firstEndpoint < endpointCount; ++firstEndpoint) {
        if (visited[firstEndpoint] || eligibleByEndpoint[firstEndpoint].empty()) {
            continue;
        }
        std::vector<int> componentEndpoints;
        std::vector<int> componentCandidates;
        std::queue<int> queue;
        queue.push(static_cast<int>(firstEndpoint));
        visited[firstEndpoint] = 1;
        std::set<int> candidateSet;
        while (!queue.empty()) {
            const int endpoint = queue.front();
            queue.pop();
            componentEndpoints.push_back(endpoint);
            for (const int candidateIndex : eligibleByEndpoint[static_cast<std::size_t>(endpoint)]) {
                candidateSet.insert(candidateIndex);
                const int other = candidateEndpointA[static_cast<std::size_t>(candidateIndex)] == endpoint
                    ? candidateEndpointB[static_cast<std::size_t>(candidateIndex)]
                    : candidateEndpointA[static_cast<std::size_t>(candidateIndex)];
                if (!visited[static_cast<std::size_t>(other)]) {
                    visited[static_cast<std::size_t>(other)] = 1;
                    queue.push(other);
                }
            }
        }
        componentCandidates.assign(candidateSet.begin(), candidateSet.end());
        ++result.stats.globalMatchingComponents;

        std::vector<int> chosen;
        if (componentEndpoints.size() <= static_cast<std::size_t>(
                options.maxExactMatchingEndpoints)) {
            ++result.stats.globalMatchingExactComponents;
            const std::size_t count = componentEndpoints.size();
            std::map<int, int> localIndex;
            for (std::size_t index = 0; index < count; ++index) {
                localIndex[componentEndpoints[index]] = static_cast<int>(index);
            }
            std::vector<std::vector<std::pair<int, int>>> incident(count);
            for (const int candidateIndex : componentCandidates) {
                const int a = localIndex[candidateEndpointA[static_cast<std::size_t>(candidateIndex)]];
                const int b = localIndex[candidateEndpointB[static_cast<std::size_t>(candidateIndex)]];
                incident[static_cast<std::size_t>(a)].push_back({b, candidateIndex});
                incident[static_cast<std::size_t>(b)].push_back({a, candidateIndex});
            }
            const std::uint32_t fullMask = (std::uint32_t{1} << count) - 1U;
            std::map<std::uint32_t, double> memo;
            std::map<std::uint32_t, int> choice;
            const auto solve = [&](auto&& self, std::uint32_t mask) -> double {
                if (mask == 0U) {
                    return 0.0;
                }
                const auto found = memo.find(mask);
                if (found != memo.end()) {
                    return found->second;
                }
                int first = 0;
                while ((mask & (std::uint32_t{1} << first)) == 0U) {
                    ++first;
                }
                const std::uint32_t withoutFirst = mask &
                    ~(std::uint32_t{1} << first);
                double best = options.noConnectCost + self(self, withoutFirst);
                int bestChoice = -1;
                for (const auto& item : incident[static_cast<std::size_t>(first)]) {
                    const int other = item.first;
                    if ((mask & (std::uint32_t{1} << other)) == 0U) {
                        continue;
                    }
                    const int candidateIndex = item.second;
                    const double edgeCost = std::max(
                        0.0, result.candidates[static_cast<std::size_t>(candidateIndex)].normalizedCost);
                    const std::uint32_t remaining = withoutFirst &
                        ~(std::uint32_t{1} << other);
                    const double total = edgeCost + self(self, remaining);
                    if (total + 1e-12 < best) {
                        best = total;
                        bestChoice = candidateIndex;
                    }
                }
                memo[mask] = best;
                choice[mask] = bestChoice;
                return best;
            };
            (void)solve(solve, fullMask);
            std::uint32_t mask = fullMask;
            while (mask != 0U) {
                int first = 0;
                while ((mask & (std::uint32_t{1} << first)) == 0U) {
                    ++first;
                }
                const int selectedCandidate = choice[mask];
                mask &= ~(std::uint32_t{1} << first);
                if (selectedCandidate < 0) {
                    ++result.stats.globalMatchingNoConnectEndpoints;
                    continue;
                }
                chosen.push_back(selectedCandidate);
                const int other = candidateEndpointA[static_cast<std::size_t>(selectedCandidate)] ==
                        componentEndpoints[static_cast<std::size_t>(first)]
                    ? candidateEndpointB[static_cast<std::size_t>(selectedCandidate)]
                    : candidateEndpointA[static_cast<std::size_t>(selectedCandidate)];
                mask &= ~(std::uint32_t{1} << localIndex[other]);
            }
        } else {
            ++result.stats.globalMatchingFallbackComponents;
            std::sort(componentCandidates.begin(), componentCandidates.end(), [&](int lhs, int rhs) {
                return std::tie(result.candidates[static_cast<std::size_t>(lhs)].normalizedCost, lhs) <
                    std::tie(result.candidates[static_cast<std::size_t>(rhs)].normalizedCost, rhs);
            });
            std::set<int> usedEndpoints;
            for (const int candidateIndex : componentCandidates) {
                const auto& candidate = result.candidates[static_cast<std::size_t>(candidateIndex)];
                if (candidate.normalizedCost >= 2.0 * options.noConnectCost ||
                    usedEndpoints.count(candidateEndpointA[static_cast<std::size_t>(candidateIndex)]) ||
                    usedEndpoints.count(candidateEndpointB[static_cast<std::size_t>(candidateIndex)])) {
                    continue;
                }
                usedEndpoints.insert(candidateEndpointA[static_cast<std::size_t>(candidateIndex)]);
                usedEndpoints.insert(candidateEndpointB[static_cast<std::size_t>(candidateIndex)]);
                chosen.push_back(candidateIndex);
            }
            result.stats.globalMatchingNoConnectEndpoints +=
                static_cast<int>(componentEndpoints.size() - usedEndpoints.size());
        }

        for (const int candidateIndex : chosen) {
            selected[static_cast<std::size_t>(candidateIndex)] = 1;
        }
    }

    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        WeakContinuityCandidate& candidate = result.candidates[index];
        if (candidate.decision != "eligible") {
            continue;
        }
        if (!selected[index]) {
            candidate.decision = "global_matching_no_connect";
            continue;
        }
        for (const SkeletonPoint& point : candidate.path) {
            result.mask.set(point.x, point.y, true);
        }
        candidate.accepted = true;
        candidate.selectedByGlobalMatching = true;
        candidate.decision = "accepted_global_matching";
        ++result.stats.acceptedConnections;
        result.stats.addedPathPixels += candidate.addedPathPixels;
    }
}

void applyGlobalNoConnectMatching(
    WeakContinuityResult& result,
    const std::vector<int>& candidateEndpointA,
    const std::vector<int>& candidateEndpointB,
    const WeakContinuityOptions& options) {
    const std::size_t endpointCount =
        static_cast<std::size_t>(result.stats.endpointCount);
    std::vector<int> eligibleCandidates;
    std::vector<std::vector<int>> eligibleByEndpoint(endpointCount);
    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        if (result.candidates[index].decision != "eligible") {
            continue;
        }
        eligibleCandidates.push_back(static_cast<int>(index));
        eligibleByEndpoint[static_cast<std::size_t>(candidateEndpointA[index])].push_back(
            static_cast<int>(index));
        eligibleByEndpoint[static_cast<std::size_t>(candidateEndpointB[index])].push_back(
            static_cast<int>(index));
    }

    std::vector<std::vector<int>> spatialConflicts(result.candidates.size());
    for (std::size_t first = 0; first < eligibleCandidates.size(); ++first) {
        const int firstIndex = eligibleCandidates[first];
        for (std::size_t second = first + 1;
             second < eligibleCandidates.size(); ++second) {
            const int secondIndex = eligibleCandidates[second];
            if (!pathsSpatiallyConflict(
                    result.candidates[static_cast<std::size_t>(firstIndex)].path,
                    result.candidates[static_cast<std::size_t>(secondIndex)].path)) {
                continue;
            }
            spatialConflicts[static_cast<std::size_t>(firstIndex)].push_back(secondIndex);
            spatialConflicts[static_cast<std::size_t>(secondIndex)].push_back(firstIndex);
            ++result.candidates[static_cast<std::size_t>(firstIndex)].spatialConflictCount;
            ++result.candidates[static_cast<std::size_t>(secondIndex)].spatialConflictCount;
            ++result.stats.spatialConflictPairs;
        }
    }
    if (result.stats.spatialConflictPairs == 0) {
        applyEndpointOnlyGlobalNoConnectMatching(
            result, candidateEndpointA, candidateEndpointB, options);
        return;
    }

    std::vector<std::vector<int>> adjacency(result.candidates.size());
    for (const std::vector<int>& incident : eligibleByEndpoint) {
        for (std::size_t first = 0; first < incident.size(); ++first) {
            for (std::size_t second = first + 1; second < incident.size(); ++second) {
                adjacency[static_cast<std::size_t>(incident[first])].push_back(
                    incident[second]);
                adjacency[static_cast<std::size_t>(incident[second])].push_back(
                    incident[first]);
            }
        }
    }
    for (const int candidateIndex : eligibleCandidates) {
        for (const int conflict : spatialConflicts[static_cast<std::size_t>(candidateIndex)]) {
            adjacency[static_cast<std::size_t>(candidateIndex)].push_back(conflict);
        }
    }

    std::vector<unsigned char> selected(result.candidates.size(), 0);
    std::vector<unsigned char> visited(result.candidates.size(), 0);
    for (const int firstCandidate : eligibleCandidates) {
        if (visited[static_cast<std::size_t>(firstCandidate)]) {
            continue;
        }
        std::vector<int> componentCandidates;
        std::set<int> endpointSet;
        std::queue<int> queue;
        queue.push(firstCandidate);
        visited[static_cast<std::size_t>(firstCandidate)] = 1;
        while (!queue.empty()) {
            const int candidateIndex = queue.front();
            queue.pop();
            componentCandidates.push_back(candidateIndex);
            endpointSet.insert(candidateEndpointA[static_cast<std::size_t>(candidateIndex)]);
            endpointSet.insert(candidateEndpointB[static_cast<std::size_t>(candidateIndex)]);
            for (const int next : adjacency[static_cast<std::size_t>(candidateIndex)]) {
                if (!visited[static_cast<std::size_t>(next)]) {
                    visited[static_cast<std::size_t>(next)] = 1;
                    queue.push(next);
                }
            }
        }
        std::vector<int> componentEndpoints(endpointSet.begin(), endpointSet.end());
        ++result.stats.globalMatchingComponents;

        std::sort(componentCandidates.begin(), componentCandidates.end(),
            [&](int lhs, int rhs) {
                const double lhsBenefit = 2.0 * options.noConnectCost -
                    std::max(0.0, result.candidates[
                        static_cast<std::size_t>(lhs)].normalizedCost);
                const double rhsBenefit = 2.0 * options.noConnectCost -
                    std::max(0.0, result.candidates[
                        static_cast<std::size_t>(rhs)].normalizedCost);
                if (std::abs(lhsBenefit - rhsBenefit) > 1e-12) {
                    return lhsBenefit > rhsBenefit;
                }
                return lhs < rhs;
            });

        std::vector<int> chosen;
        bool exact = componentEndpoints.size() <= static_cast<std::size_t>(
                options.maxExactMatchingEndpoints) &&
            componentCandidates.size() <= 30U;
        if (exact) {
            std::map<int, int> localEndpointIndex;
            for (std::size_t index = 0; index < componentEndpoints.size(); ++index) {
                localEndpointIndex[componentEndpoints[index]] = static_cast<int>(index);
            }
            std::vector<double> benefit(componentCandidates.size(), 0.0);
            std::vector<double> suffix(componentCandidates.size() + 1, 0.0);
            for (std::size_t index = 0; index < componentCandidates.size(); ++index) {
                benefit[index] = std::max(0.0, 2.0 * options.noConnectCost -
                    std::max(0.0, result.candidates[
                        static_cast<std::size_t>(componentCandidates[index])].normalizedCost));
            }
            for (std::size_t index = componentCandidates.size(); index > 0; --index) {
                suffix[index - 1] = suffix[index] + benefit[index - 1];
            }

            double bestBenefit = 0.0;
            std::vector<int> current;
            std::uint64_t searchNodes = 0;
            bool exhaustedBudget = false;
            constexpr std::uint64_t kMaximumSearchNodes = 2000000;
            const auto search = [&](auto&& self,
                                    std::size_t position,
                                    std::uint32_t usedEndpoints,
                                    double currentBenefit) -> void {
                if (++searchNodes > kMaximumSearchNodes) {
                    exhaustedBudget = true;
                    return;
                }
                if (position == componentCandidates.size()) {
                    if (currentBenefit > bestBenefit + 1e-12) {
                        bestBenefit = currentBenefit;
                        chosen = current;
                    }
                    return;
                }
                if (currentBenefit + suffix[position] <= bestBenefit + 1e-12) {
                    return;
                }

                const int candidateIndex = componentCandidates[position];
                const int localA = localEndpointIndex[
                    candidateEndpointA[static_cast<std::size_t>(candidateIndex)]];
                const int localB = localEndpointIndex[
                    candidateEndpointB[static_cast<std::size_t>(candidateIndex)]];
                const std::uint32_t endpointBits =
                    (std::uint32_t{1} << localA) |
                    (std::uint32_t{1} << localB);
                bool conflicts = (usedEndpoints & endpointBits) != 0U;
                if (!conflicts && benefit[position] > 0.0) {
                    const auto& candidateConflicts =
                        spatialConflicts[static_cast<std::size_t>(candidateIndex)];
                    for (const int selectedIndex : current) {
                        if (std::find(candidateConflicts.begin(),
                                      candidateConflicts.end(),
                                      selectedIndex) != candidateConflicts.end()) {
                            conflicts = true;
                            break;
                        }
                    }
                }
                if (!conflicts && benefit[position] > 0.0) {
                    current.push_back(candidateIndex);
                    self(self, position + 1, usedEndpoints | endpointBits,
                         currentBenefit + benefit[position]);
                    current.pop_back();
                }
                self(self, position + 1, usedEndpoints, currentBenefit);
            };
            search(search, 0, 0U, 0.0);
            if (exhaustedBudget) {
                exact = false;
                chosen.clear();
            } else {
                ++result.stats.globalMatchingExactComponents;
            }
        }

        if (!exact) {
            ++result.stats.globalMatchingFallbackComponents;
            std::set<int> usedEndpoints;
            for (const int candidateIndex : componentCandidates) {
                const WeakContinuityCandidate& candidate =
                    result.candidates[static_cast<std::size_t>(candidateIndex)];
                if (candidate.normalizedCost >= 2.0 * options.noConnectCost ||
                    usedEndpoints.count(candidateEndpointA[
                        static_cast<std::size_t>(candidateIndex)]) ||
                    usedEndpoints.count(candidateEndpointB[
                        static_cast<std::size_t>(candidateIndex)])) {
                    continue;
                }
                bool conflicts = false;
                const auto& candidateConflicts =
                    spatialConflicts[static_cast<std::size_t>(candidateIndex)];
                for (const int selectedIndex : chosen) {
                    if (std::find(candidateConflicts.begin(),
                                  candidateConflicts.end(),
                                  selectedIndex) != candidateConflicts.end()) {
                        conflicts = true;
                        break;
                    }
                }
                if (conflicts) {
                    continue;
                }
                usedEndpoints.insert(candidateEndpointA[
                    static_cast<std::size_t>(candidateIndex)]);
                usedEndpoints.insert(candidateEndpointB[
                    static_cast<std::size_t>(candidateIndex)]);
                chosen.push_back(candidateIndex);
            }
        }

        for (const int candidateIndex : chosen) {
            selected[static_cast<std::size_t>(candidateIndex)] = 1;
        }
        result.stats.globalMatchingNoConnectEndpoints += static_cast<int>(
            componentEndpoints.size() - 2U * chosen.size());
    }

    for (std::size_t index = 0; index < result.candidates.size(); ++index) {
        WeakContinuityCandidate& candidate = result.candidates[index];
        if (candidate.decision != "eligible") {
            continue;
        }
        if (!selected[index]) {
            bool rejectedBySpatialConflict = false;
            for (const int conflict : spatialConflicts[index]) {
                if (selected[static_cast<std::size_t>(conflict)]) {
                    rejectedBySpatialConflict = true;
                    break;
                }
            }
            if (rejectedBySpatialConflict) {
                candidate.decision = "global_matching_spatial_conflict";
                ++result.stats.spatialConflictRejected;
            } else {
                candidate.decision = "global_matching_no_connect";
            }
            continue;
        }
        for (const SkeletonPoint& point : candidate.path) {
            result.mask.set(point.x, point.y, true);
        }
        candidate.accepted = true;
        candidate.selectedByGlobalMatching = true;
        candidate.decision = "accepted_global_matching";
        ++result.stats.acceptedConnections;
        result.stats.addedPathPixels += candidate.addedPathPixels;
    }
}

} // namespace

void validateWeakContinuityOptions(const WeakContinuityOptions& options) {
    const bool finiteValues =
        std::isfinite(options.maxEndpointAngleDegrees) &&
        std::isfinite(options.maxWidthRatio) &&
        std::isfinite(options.minAttributeRatio) &&
        std::isfinite(options.localContrastRatio) &&
        std::isfinite(options.minSupportRatio) &&
        std::isfinite(options.minMeanSupport) &&
        std::isfinite(options.maxPathLengthRatio) &&
        std::isfinite(options.maxTurnAngleDegrees) &&
        std::isfinite(options.minCompetitionMargin) &&
        std::isfinite(options.attributeWeight) &&
        std::isfinite(options.directionWeight) &&
        std::isfinite(options.curvatureWeight);
    if (!finiteValues) {
        throw std::runtime_error("Weak continuity numeric options must be finite.");
    }
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
        !std::isfinite(options.noConnectCost) || options.noConnectCost < 0.0 ||
        options.maxExactMatchingEndpoints < 2 || options.maxEvidenceRecords < 1) {
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
    if (!std::isfinite(supportThreshold)) {
        throw std::runtime_error("Weak continuity support threshold must be finite.");
    }
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
    const auto tangents = endpointTangents(
        grid, graph, options.tangentWindow, options.usePhysicalMetric);
    const double maxEndpointDistance = metricRadius(
        grid, options.maxDistance, options.usePhysicalMetric);
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
            const double endpointDistanceWorld = metricDistance(
                grid, a, b, true);
            const double endpointDistance = options.usePhysicalMetric
                ? endpointDistanceWorld : distance(a, b);
            if (endpointDistance > (options.usePhysicalMetric
                                    ? maxEndpointDistance
                                    : static_cast<double>(options.maxDistance))) {
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
            const Vec2 gapAB = metricVector(
                grid, a, b, options.usePhysicalMetric);
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
            seed.geometricScore = endpointDistance /
                    std::max(1e-9, options.usePhysicalMetric
                        ? maxEndpointDistance : static_cast<double>(options.maxDistance)) +
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
        candidate.endpointDistanceWorld = metricDistance(
            grid, candidate.endpointA, candidate.endpointB, true);
        candidate.angleA = seed.angleA;
        candidate.angleB = seed.angleB;
        candidate.widthA = seed.widthA;
        candidate.widthB = seed.widthB;
        const BackgroundEstimate background = localBackgroundValue(
            grid, candidate.endpointA, candidate.endpointB,
            options.corridorRadius, options.usePhysicalMetric);
        candidate.finiteBackgroundSamples = background.finiteSamples;
        candidate.invalidBackgroundSamples = background.invalidSamples;
        result.stats.finiteBackgroundSamples += background.finiteSamples;
        result.stats.invalidBackgroundSamples += background.invalidSamples;
        if (!background.valid) {
            candidate.decision = "invalid_local_background";
            ++result.stats.invalidBackgroundRejected;
            result.candidates.push_back(std::move(candidate));
            continue;
        }
        double totalCost = 0.0;
        ++result.stats.searchedPaths;
        candidate.path = findPath(
            grid, skeleton, candidate.endpointA, candidate.endpointB,
            supportThreshold, background.value, options, &totalCost);
        candidate.pathFound = !candidate.path.empty();
        if (!candidate.pathFound) {
            candidate.decision = "no_path_or_third_party_skeleton";
            ++result.stats.noPathRejected;
            result.candidates.push_back(std::move(candidate));
            continue;
        }
        ++result.stats.foundPaths;
        measurePath(candidate, grid, skeleton, supportThreshold, background.value, options);
        result.stats.evaluatedSupportPixels += candidate.evaluatedSupportPixels;
        result.stats.excludedSkeletonSupportPixels +=
            candidate.excludedSkeletonSupportPixels;
        result.stats.invalidPathSamples += candidate.invalidPathSamples;
        candidate.normalizedCost = totalCost / std::max(1.0, candidate.pathLength);
        if (candidate.evaluatedSupportPixels <= 0) {
            candidate.decision = "no_new_path_support_samples";
            ++result.stats.noEvaluatedSupportRejected;
        } else if (candidate.supportRatio < options.minSupportRatio ||
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

    if (options.useGlobalMatching) {
        applyGlobalNoConnectMatching(
            result, candidateEndpointA, candidateEndpointB, options);
    } else {
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
    }

    if (result.candidates.size() > static_cast<std::size_t>(options.maxEvidenceRecords)) {
        result.candidates.resize(static_cast<std::size_t>(options.maxEvidenceRecords));
    }
    return result;
}

} // namespace fpt
