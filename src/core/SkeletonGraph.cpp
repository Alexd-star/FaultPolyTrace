#include "SkeletonGraph.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace fpt {
namespace {

struct Pixel {
    int x = 0;
    int y = 0;

    bool operator<(const Pixel& other) const {
        if (x == other.x) {
            return y < other.y;
        }
        return x < other.x;
    }

    bool operator==(const Pixel& other) const {
        return x == other.x && y == other.y;
    }
};

struct NeighborPixels {
    Pixel values[8];
    int count = 0;

    const Pixel* begin() const { return values; }
    const Pixel* end() const { return values + count; }
};

NeighborPixels neighbors8(const BinaryMask& mask, int x, int y) {
    NeighborPixels out;
    for (int yy = y - 1; yy <= y + 1; ++yy) {
        for (int xx = x - 1; xx <= x + 1; ++xx) {
            if (xx == x && yy == y) {
                continue;
            }
            if (mask.get(xx, yy)) {
                out.values[out.count++] = {xx, yy};
            }
        }
    }
    return out;
}

SkeletonPoint toPoint(const Pixel& pixel) {
    return {pixel.x, pixel.y};
}

double distance(const SkeletonPoint& a, const SkeletonPoint& b) {
    const double dx = static_cast<double>(a.x - b.x);
    const double dy = static_cast<double>(a.y - b.y);
    return std::sqrt(dx * dx + dy * dy);
}

double dot(double ax, double ay, double bx, double by) {
    return ax * bx + ay * by;
}

double norm(double x, double y) {
    return std::sqrt(x * x + y * y);
}

double angleDegreesBetween(double ax, double ay, double bx, double by) {
    const double denom = norm(ax, ay) * norm(bx, by);
    if (denom <= 1e-12) {
        return 180.0;
    }
    double c = dot(ax, ay, bx, by) / denom;
    c = std::max(-1.0, std::min(1.0, c));
    return std::acos(c) * 180.0 / 3.14159265358979323846;
}

void drawLine(BinaryMask& mask, const SkeletonPoint& a, const SkeletonPoint& b) {
    int x0 = a.x;
    int y0 = a.y;
    const int x1 = b.x;
    const int y1 = b.y;
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        mask.set(x0, y0, true);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

} // namespace

SkeletonGraph buildSkeletonGraph(const BinaryMask& skeleton) {
    SkeletonGraph graph;
    const int width = skeleton.width();
    const int height = skeleton.height();
    auto position = [width](int x, int y) {
        return static_cast<std::size_t>(y * width + x);
    };
    std::vector<unsigned char> nodeTypes(
        static_cast<std::size_t>(width * height), 0);
    std::vector<Pixel> nodes;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!skeleton.get(x, y)) {
                continue;
            }
            const int degree = neighbors8(skeleton, x, y).count;
            if (degree == 1) {
                graph.endpoints.push_back({x, y});
                nodeTypes[position(x, y)] = 1;
                nodes.push_back({x, y});
            } else if (degree != 2) {
                graph.junctions.push_back({x, y});
                nodeTypes[position(x, y)] = 2;
                nodes.push_back({x, y});
            }
        }
    }
    std::sort(nodes.begin(), nodes.end());

    std::vector<unsigned char> visitedEdges(
        static_cast<std::size_t>(width * height), 0);
    auto directionIndex = [](const Pixel& a, const Pixel& b) {
        int index = 0;
        for (int yy = -1; yy <= 1; ++yy) {
            for (int xx = -1; xx <= 1; ++xx) {
                if (xx == 0 && yy == 0) {
                    continue;
                }
                if (a.x + xx == b.x && a.y + yy == b.y) {
                    return index;
                }
                ++index;
            }
        }
        return -1;
    };
    auto markVisited = [&](const Pixel& a, const Pixel& b) {
        const int direction = directionIndex(a, b);
        if (direction < 0) {
            return;
        }
        visitedEdges[position(a.x, a.y)] |=
            static_cast<unsigned char>(1U << direction);
        visitedEdges[position(b.x, b.y)] |=
            static_cast<unsigned char>(1U << (7 - direction));
    };
    auto wasVisited = [&](const Pixel& a, const Pixel& b) {
        const int direction = directionIndex(a, b);
        return direction >= 0 &&
            (visitedEdges[position(a.x, a.y)] &
             static_cast<unsigned char>(1U << direction)) != 0;
    };

    int nextBranchId = 1;
    for (const Pixel& node : nodes) {
        for (const Pixel& firstNeighbor : neighbors8(skeleton, node.x, node.y)) {
            if (wasVisited(node, firstNeighbor)) {
                continue;
            }

            SkeletonBranch branch;
            branch.id = nextBranchId++;
            branch.startIsJunction = nodeTypes[position(node.x, node.y)] == 2;
            branch.points.push_back(toPoint(node));

            Pixel previous = node;
            Pixel current = firstNeighbor;
            markVisited(previous, current);

            while (true) {
                branch.points.push_back(toPoint(current));
                const unsigned char currentNodeType =
                    nodeTypes[position(current.x, current.y)];
                if (currentNodeType != 0) {
                    branch.endIsJunction = currentNodeType == 2;
                    break;
                }

                const NeighborPixels nextCandidates =
                    neighbors8(skeleton, current.x, current.y);
                Pixel next;
                bool hasNext = false;
                for (const Pixel& candidate : nextCandidates) {
                    if (candidate == previous) {
                        continue;
                    }
                    if (!hasNext) {
                        next = candidate;
                        hasNext = true;
                    }
                    if (!wasVisited(current, candidate)) {
                        next = candidate;
                        break;
                    }
                }
                if (!hasNext || wasVisited(current, next)) {
                    break;
                }
                previous = current;
                current = next;
                markVisited(previous, current);
            }

            if (branch.points.size() >= 2) {
                graph.branches.push_back(std::move(branch));
            }
        }
    }

    // Closed loops have no endpoints or junctions. Preserve them as one branch.
    if (nodes.empty()) {
        std::vector<unsigned char> visited(
            static_cast<std::size_t>(width * height), 0);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                Pixel start{x, y};
                if (!skeleton.get(x, y) || visited[position(x, y)] != 0) {
                    continue;
                }
                SkeletonBranch branch;
                branch.id = nextBranchId++;
                Pixel previous{-999999, -999999};
                Pixel current = start;
                while (visited[position(current.x, current.y)] == 0) {
                    visited[position(current.x, current.y)] = 1;
                    branch.points.push_back(toPoint(current));
                    const NeighborPixels candidates =
                        neighbors8(skeleton, current.x, current.y);
                    Pixel next;
                    bool hasNext = false;
                    for (const Pixel& candidate : candidates) {
                        if (!(candidate == previous)) {
                            next = candidate;
                            hasNext = true;
                            break;
                        }
                    }
                    if (!hasNext) {
                        break;
                    }
                    previous = current;
                    current = next;
                }
                if (branch.points.size() >= 2) {
                    graph.branches.push_back(std::move(branch));
                }
            }
        }
    }

    return graph;
}

BinaryMask branchesToMask(const SkeletonGraph& graph, int width, int height, int minBranchLength, int minJunctionBranchLength) {
    return branchesToMaskWithStats(graph, width, height, minBranchLength, minJunctionBranchLength).mask;
}

BranchMaskResult branchesToMaskWithStats(
    const SkeletonGraph& graph,
    int width,
    int height,
    int minBranchLength,
    int minJunctionBranchLength) {
    BranchMaskResult result{BinaryMask(width, height), 0, 0};
    for (const SkeletonBranch& branch : graph.branches) {
        const int pointCount = static_cast<int>(branch.points.size());
        const bool touchesJunction = branch.startIsJunction || branch.endIsJunction;
        const bool keepByLength = pointCount >= minBranchLength;
        const bool keepByJunction = touchesJunction && pointCount >= minJunctionBranchLength;
        if (!keepByLength && !keepByJunction) {
            ++result.prunedBranches;
            continue;
        }
        ++result.keptBranches;
        for (const SkeletonPoint& point : branch.points) {
            result.mask.set(point.x, point.y, true);
        }
    }
    return result;
}

BinaryMask connectEndpointGaps(const BinaryMask& skeleton, const SkeletonGraph& graph, int maxGapDistance, double maxAngleDegrees) {
    return connectEndpointGapsWithStats(skeleton, graph, maxGapDistance, maxAngleDegrees).mask;
}

GapConnectionResult connectEndpointGapsWithStats(const BinaryMask& skeleton, const SkeletonGraph& graph, int maxGapDistance, double maxAngleDegrees) {
    return connectEndpointGapsWithStats(skeleton, graph, maxGapDistance, maxAngleDegrees, nullptr, nullptr);
}

GapConnectionResult connectEndpointGapsWithStats(
    const BinaryMask& skeleton,
    const SkeletonGraph& graph,
    int maxGapDistance,
    double maxAngleDegrees,
    GapSupportPredicate supportPredicate,
    void* supportContext) {
    GapConnectionResult result{skeleton, 0};
    if (maxGapDistance <= 0 || graph.endpoints.size() < 2) {
        return result;
    }

    std::map<std::pair<int, int>, SkeletonPoint> endpointDirections;
    for (const SkeletonBranch& branch : graph.branches) {
        if (branch.points.size() < 2) {
            continue;
        }
        const SkeletonPoint& a0 = branch.points.front();
        const SkeletonPoint& a1 = branch.points[1];
        endpointDirections[{a0.x, a0.y}] = {a0.x - a1.x, a0.y - a1.y};

        const SkeletonPoint& b0 = branch.points.back();
        const SkeletonPoint& b1 = branch.points[branch.points.size() - 2];
        endpointDirections[{b0.x, b0.y}] = {b0.x - b1.x, b0.y - b1.y};
    }

    std::vector<bool> used(graph.endpoints.size(), false);
    for (std::size_t i = 0; i < graph.endpoints.size(); ++i) {
        if (used[i]) {
            continue;
        }

        int bestJ = -1;
        double bestDistance = static_cast<double>(maxGapDistance) + 1.0;
        const SkeletonPoint& a = graph.endpoints[i];
        auto dirAIt = endpointDirections.find({a.x, a.y});
        if (dirAIt == endpointDirections.end()) {
            continue;
        }

        for (std::size_t j = i + 1; j < graph.endpoints.size(); ++j) {
            if (used[j]) {
                continue;
            }
            const SkeletonPoint& b = graph.endpoints[j];
            const double d = distance(a, b);
            if (d > maxGapDistance || d >= bestDistance) {
                continue;
            }
            auto dirBIt = endpointDirections.find({b.x, b.y});
            if (dirBIt == endpointDirections.end()) {
                continue;
            }

            const double gapABx = static_cast<double>(b.x - a.x);
            const double gapABy = static_cast<double>(b.y - a.y);
            const double gapBAx = -gapABx;
            const double gapBAy = -gapABy;
            const SkeletonPoint dirA = dirAIt->second;
            const SkeletonPoint dirB = dirBIt->second;
            const double angleA = angleDegreesBetween(dirA.x, dirA.y, gapABx, gapABy);
            const double angleB = angleDegreesBetween(dirB.x, dirB.y, gapBAx, gapBAy);

            if (angleA <= maxAngleDegrees && angleB <= maxAngleDegrees &&
                (!supportPredicate || supportPredicate(a, b, supportContext))) {
                bestDistance = d;
                bestJ = static_cast<int>(j);
            }
        }

        if (bestJ >= 0) {
            drawLine(result.mask, graph.endpoints[i], graph.endpoints[static_cast<std::size_t>(bestJ)]);
            used[i] = true;
            used[static_cast<std::size_t>(bestJ)] = true;
            ++result.connectedGaps;
        }
    }

    return result;
}

} // namespace fpt
