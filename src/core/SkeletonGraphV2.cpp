#include "SkeletonGraphV2.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

namespace fpt {
namespace {

int indexOf(int width, int x, int y) {
    return y * width + x;
}

bool pointLess(const SkeletonPoint& first, const SkeletonPoint& second) {
    return first.y != second.y ? first.y < second.y : first.x < second.x;
}

bool samePoint(const SkeletonPoint& first, const SkeletonPoint& second) {
    return first.x == second.x && first.y == second.y;
}

std::vector<SkeletonPoint> activeNeighbors(const BinaryMask& mask, int x, int y) {
    std::vector<SkeletonPoint> neighbors;
    for (int yy = y - 1; yy <= y + 1; ++yy) {
        for (int xx = x - 1; xx <= x + 1; ++xx) {
            if ((xx != x || yy != y) && mask.get(xx, yy)) {
                neighbors.push_back({xx, yy});
            }
        }
    }
    std::sort(neighbors.begin(), neighbors.end(), pointLess);
    return neighbors;
}

SkeletonNodeV2 makeNode(
    int id,
    SkeletonNodeTypeV2 type,
    std::vector<SkeletonPoint> members,
    const GridMetric& metric) {
    std::sort(members.begin(), members.end(), pointLess);
    SkeletonNodeV2 node;
    node.id = id;
    node.type = type;
    node.members = std::move(members);
    for (const SkeletonPoint& point : node.members) {
        node.centerGridX += static_cast<double>(point.x);
        node.centerGridY += static_cast<double>(point.y);
    }
    const double count = static_cast<double>(node.members.size());
    if (count > 0.0) {
        node.centerGridX /= count;
        node.centerGridY /= count;
    }
    node.centerWorldX = metric.worldX(node.centerGridX + 0.5);
    node.centerWorldY = metric.worldY(node.centerGridY + 0.5);
    return node;
}

SkeletonPoint nearestNodeMember(
    const SkeletonNodeV2& node,
    const SkeletonPoint& outside,
    const GridMetric& metric) {
    SkeletonPoint best = node.members.empty() ? outside : node.members.front();
    double bestDistance = std::numeric_limits<double>::infinity();
    for (const SkeletonPoint& member : node.members) {
        const double distance = metric.distance(
            static_cast<double>(member.x), static_cast<double>(member.y),
            static_cast<double>(outside.x), static_cast<double>(outside.y));
        if (distance < bestDistance - 1e-12 ||
            (std::abs(distance - bestDistance) <= 1e-12 && pointLess(member, best))) {
            bestDistance = distance;
            best = member;
        }
    }
    return best;
}

std::vector<int> adjacentNodeIds(
    const BinaryMask& skeleton,
    const std::vector<int>& nodeByPixel,
    const SkeletonPoint& point) {
    std::vector<int> ids;
    for (const SkeletonPoint& neighbor : activeNeighbors(skeleton, point.x, point.y)) {
        const int id = nodeByPixel[static_cast<std::size_t>(
            indexOf(skeleton.width(), neighbor.x, neighbor.y))];
        if (id > 0) {
            ids.push_back(id);
        }
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

std::vector<SkeletonPoint> orderComponent(
    const std::vector<SkeletonPoint>& component,
    const std::vector<int>& componentLabel,
    int label,
    int width,
    int height,
    bool& closed) {
    auto internalNeighbors = [&](const SkeletonPoint& point) {
        std::vector<SkeletonPoint> neighbors;
        for (int yy = point.y - 1; yy <= point.y + 1; ++yy) {
            for (int xx = point.x - 1; xx <= point.x + 1; ++xx) {
                if (xx < 0 || yy < 0 || xx >= width || yy >= height ||
                    (xx == point.x && yy == point.y)) {
                    continue;
                }
                if (componentLabel[static_cast<std::size_t>(indexOf(width, xx, yy))] == label) {
                    neighbors.push_back({xx, yy});
                }
            }
        }
        std::sort(neighbors.begin(), neighbors.end(), pointLess);
        return neighbors;
    };

    SkeletonPoint start = component.front();
    closed = true;
    for (const SkeletonPoint& point : component) {
        const std::size_t degree = internalNeighbors(point).size();
        if (degree <= 1) {
            if (closed || pointLess(point, start)) {
                start = point;
            }
            closed = false;
        }
    }
    if (closed) {
        start = *std::min_element(component.begin(), component.end(), pointLess);
    }

    std::vector<SkeletonPoint> ordered;
    ordered.reserve(component.size());
    std::vector<unsigned char> visited(componentLabel.size(), 0);
    SkeletonPoint previous{-1, -1};
    SkeletonPoint current = start;
    while (ordered.size() < component.size()) {
        ordered.push_back(current);
        visited[static_cast<std::size_t>(indexOf(width, current.x, current.y))] = 1;
        const std::vector<SkeletonPoint> neighbors = internalNeighbors(current);
        bool advanced = false;
        for (const SkeletonPoint& neighbor : neighbors) {
            if (samePoint(neighbor, previous) ||
                visited[static_cast<std::size_t>(indexOf(width, neighbor.x, neighbor.y))]) {
                continue;
            }
            previous = current;
            current = neighbor;
            advanced = true;
            break;
        }
        if (!advanced) {
            break;
        }
    }
    return ordered;
}

double branchLength(
    const std::vector<SkeletonPoint>& points,
    bool closed,
    const GridMetric& metric) {
    double length = 0.0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        length += metric.distance(points[i - 1].x, points[i - 1].y,
                                  points[i].x, points[i].y);
    }
    if (closed && points.size() > 1) {
        length += metric.distance(points.back().x, points.back().y,
                                  points.front().x, points.front().y);
    }
    return length;
}

void normalizedDirection(
    const SkeletonPoint& anchor,
    const SkeletonPoint& exit,
    const GridMetric& metric,
    double& x,
    double& y) {
    x = metric.worldDeltaX(static_cast<double>(exit.x - anchor.x));
    y = metric.worldDeltaY(static_cast<double>(exit.y - anchor.y));
    const double length = std::hypot(x, y);
    if (length > 1e-12) {
        x /= length;
        y /= length;
    }
}

} // namespace

SkeletonGraphV2 buildSkeletonGraphV2(
    const BinaryMask& skeleton,
    const GridMetric& metric) {
    if (skeleton.width() != metric.nx() || skeleton.height() != metric.ny()) {
        throw std::runtime_error("SkeletonGraphV2 mask and grid dimensions must match.");
    }

    SkeletonGraphV2 graph;
    const int width = skeleton.width();
    const int height = skeleton.height();
    const std::size_t pixelCount = skeleton.values().size();
    std::vector<int> degree(pixelCount, 0);
    std::vector<unsigned char> junctionCandidate(pixelCount, 0);
    std::vector<int> nodeByPixel(pixelCount, 0);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!skeleton.get(x, y)) {
                continue;
            }
            ++graph.stats.skeletonPixels;
            const int value = static_cast<int>(activeNeighbors(skeleton, x, y).size());
            degree[static_cast<std::size_t>(indexOf(width, x, y))] = value;
            if (value >= 3) {
                junctionCandidate[static_cast<std::size_t>(indexOf(width, x, y))] = 1;
                ++graph.stats.rawJunctionPixels;
            }
        }
    }

    int nextNodeId = 1;
    std::vector<unsigned char> visitedJunction(pixelCount, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixelIndex = indexOf(width, x, y);
            if (!junctionCandidate[static_cast<std::size_t>(pixelIndex)] ||
                visitedJunction[static_cast<std::size_t>(pixelIndex)]) {
                continue;
            }
            std::deque<SkeletonPoint> queue;
            std::vector<SkeletonPoint> members;
            queue.push_back({x, y});
            visitedJunction[static_cast<std::size_t>(pixelIndex)] = 1;
            while (!queue.empty()) {
                const SkeletonPoint current = queue.front();
                queue.pop_front();
                members.push_back(current);
                for (const SkeletonPoint& neighbor : activeNeighbors(
                         skeleton, current.x, current.y)) {
                    const int neighborIndex = indexOf(width, neighbor.x, neighbor.y);
                    if (junctionCandidate[static_cast<std::size_t>(neighborIndex)] &&
                        !visitedJunction[static_cast<std::size_t>(neighborIndex)]) {
                        visitedJunction[static_cast<std::size_t>(neighborIndex)] = 1;
                        queue.push_back(neighbor);
                    }
                }
            }
            SkeletonNodeV2 node = makeNode(
                nextNodeId++, SkeletonNodeTypeV2::JunctionCluster,
                std::move(members), metric);
            for (const SkeletonPoint& member : node.members) {
                nodeByPixel[static_cast<std::size_t>(
                    indexOf(width, member.x, member.y))] = node.id;
            }
            graph.nodes.push_back(std::move(node));
            ++graph.stats.junctionClusters;
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixelIndex = indexOf(width, x, y);
            if (!skeleton.get(x, y) ||
                nodeByPixel[static_cast<std::size_t>(pixelIndex)] != 0) {
                continue;
            }
            const int value = degree[static_cast<std::size_t>(pixelIndex)];
            if (value != 0 && value != 1) {
                continue;
            }
            const SkeletonNodeTypeV2 type = value == 0
                ? SkeletonNodeTypeV2::Isolated : SkeletonNodeTypeV2::Endpoint;
            SkeletonNodeV2 node = makeNode(nextNodeId++, type, {{x, y}}, metric);
            nodeByPixel[static_cast<std::size_t>(pixelIndex)] = node.id;
            graph.nodes.push_back(std::move(node));
            if (value == 0) {
                ++graph.stats.isolatedPixels;
            } else {
                ++graph.stats.endpointPixels;
            }
        }
    }

    std::vector<int> componentLabel(pixelCount, 0);
    std::vector<std::vector<SkeletonPoint>> components;
    int nextLabel = 1;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int pixelIndex = indexOf(width, x, y);
            if (!skeleton.get(x, y) ||
                nodeByPixel[static_cast<std::size_t>(pixelIndex)] != 0 ||
                componentLabel[static_cast<std::size_t>(pixelIndex)] != 0) {
                continue;
            }
            std::deque<SkeletonPoint> queue;
            std::vector<SkeletonPoint> component;
            queue.push_back({x, y});
            componentLabel[static_cast<std::size_t>(pixelIndex)] = nextLabel;
            while (!queue.empty()) {
                const SkeletonPoint current = queue.front();
                queue.pop_front();
                component.push_back(current);
                for (const SkeletonPoint& neighbor : activeNeighbors(
                         skeleton, current.x, current.y)) {
                    const int neighborIndex = indexOf(width, neighbor.x, neighbor.y);
                    if (nodeByPixel[static_cast<std::size_t>(neighborIndex)] == 0 &&
                        componentLabel[static_cast<std::size_t>(neighborIndex)] == 0) {
                        componentLabel[static_cast<std::size_t>(neighborIndex)] = nextLabel;
                        queue.push_back(neighbor);
                    }
                }
            }
            std::sort(component.begin(), component.end(), pointLess);
            components.push_back(std::move(component));
            ++nextLabel;
        }
    }

    auto nodeById = [&](int id) -> SkeletonNodeV2& {
        return graph.nodes.at(static_cast<std::size_t>(id - 1));
    };
    int nextBranchId = 1;
    int nextPortId = 1;
    int nextHalfEdgeId = 1;
    for (std::size_t componentIndex = 0; componentIndex < components.size(); ++componentIndex) {
        bool closed = false;
        std::vector<SkeletonPoint> ordered = orderComponent(
            components[componentIndex], componentLabel,
            static_cast<int>(componentIndex + 1), width, height, closed);
        if (ordered.empty()) {
            continue;
        }

        std::vector<int> startNodes = adjacentNodeIds(
            skeleton, nodeByPixel, ordered.front());
        std::vector<int> endNodes = adjacentNodeIds(
            skeleton, nodeByPixel, ordered.back());
        int startNodeId = startNodes.empty() ? 0 : startNodes.front();
        int endNodeId = endNodes.empty() ? 0 : endNodes.front();

        if (ordered.size() == 1) {
            std::vector<int> combined = startNodes;
            combined.insert(combined.end(), endNodes.begin(), endNodes.end());
            std::sort(combined.begin(), combined.end());
            combined.erase(std::unique(combined.begin(), combined.end()), combined.end());
            if (combined.size() >= 2) {
                startNodeId = combined[0];
                endNodeId = combined[1];
            }
        }
        if (startNodes.size() > 1 || endNodes.size() > 1) {
            ++graph.stats.ambiguousPortAttachments;
        }

        if (startNodeId == 0 && endNodeId == 0 && closed) {
            SkeletonNodeV2 loopNode = makeNode(
                nextNodeId++, SkeletonNodeTypeV2::Loop,
                {ordered.front()}, metric);
            startNodeId = loopNode.id;
            endNodeId = loopNode.id;
            graph.nodes.push_back(std::move(loopNode));
            ++graph.stats.loopNodes;
        }
        if (startNodeId == 0) {
            SkeletonNodeV2 endpoint = makeNode(
                nextNodeId++, SkeletonNodeTypeV2::Endpoint,
                {ordered.front()}, metric);
            startNodeId = endpoint.id;
            graph.nodes.push_back(std::move(endpoint));
            ++graph.stats.syntheticEndpointNodes;
        }
        if (endNodeId == 0) {
            SkeletonNodeV2 endpoint = makeNode(
                nextNodeId++, SkeletonNodeTypeV2::Endpoint,
                {ordered.back()}, metric);
            endNodeId = endpoint.id;
            graph.nodes.push_back(std::move(endpoint));
            ++graph.stats.syntheticEndpointNodes;
        }

        SkeletonBranchV2 branch;
        branch.id = nextBranchId++;
        branch.startNodeId = startNodeId;
        branch.endNodeId = endNodeId;
        branch.closedLoop = closed && startNodeId == endNodeId;
        branch.points = ordered;
        branch.physicalLength = branchLength(branch.points, branch.closedLoop, metric);

        SkeletonPortV2 startPort;
        startPort.id = nextPortId++;
        startPort.nodeId = startNodeId;
        startPort.branchId = branch.id;
        startPort.anchor = nearestNodeMember(
            nodeById(startNodeId), ordered.front(), metric);
        startPort.exitPoint = branch.closedLoop && ordered.size() > 1
            ? ordered[1] : ordered.front();
        normalizedDirection(startPort.anchor, startPort.exitPoint, metric,
                            startPort.directionX, startPort.directionY);

        SkeletonPortV2 endPort;
        endPort.id = nextPortId++;
        endPort.nodeId = endNodeId;
        endPort.branchId = branch.id;
        endPort.anchor = nearestNodeMember(
            nodeById(endNodeId), ordered.back(), metric);
        endPort.exitPoint = branch.closedLoop && ordered.size() > 1
            ? ordered.back() : ordered.back();
        normalizedDirection(endPort.anchor, endPort.exitPoint, metric,
                            endPort.directionX, endPort.directionY);
        if (!branch.closedLoop) {
            branch.physicalLength += metric.distance(
                startPort.anchor.x, startPort.anchor.y,
                startPort.exitPoint.x, startPort.exitPoint.y);
            branch.physicalLength += metric.distance(
                endPort.exitPoint.x, endPort.exitPoint.y,
                endPort.anchor.x, endPort.anchor.y);
        }

        SkeletonHalfEdgeV2 forward;
        forward.id = nextHalfEdgeId++;
        forward.branchId = branch.id;
        forward.originNodeId = startNodeId;
        forward.targetNodeId = endNodeId;
        forward.portId = startPort.id;
        forward.physicalLength = branch.physicalLength;
        SkeletonHalfEdgeV2 reverse;
        reverse.id = nextHalfEdgeId++;
        reverse.branchId = branch.id;
        reverse.originNodeId = endNodeId;
        reverse.targetNodeId = startNodeId;
        reverse.portId = endPort.id;
        reverse.physicalLength = branch.physicalLength;
        forward.twinId = reverse.id;
        reverse.twinId = forward.id;
        startPort.halfEdgeId = forward.id;
        endPort.halfEdgeId = reverse.id;

        branch.startPortId = startPort.id;
        branch.endPortId = endPort.id;
        branch.forwardHalfEdgeId = forward.id;
        branch.reverseHalfEdgeId = reverse.id;
        nodeById(startNodeId).portIds.push_back(startPort.id);
        nodeById(endNodeId).portIds.push_back(endPort.id);
        graph.ports.push_back(startPort);
        graph.ports.push_back(endPort);
        graph.halfEdges.push_back(forward);
        graph.halfEdges.push_back(reverse);
        graph.branches.push_back(std::move(branch));
    }

    std::set<std::pair<int, int>> directNodePairs;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int firstNodeId = nodeByPixel[static_cast<std::size_t>(indexOf(width, x, y))];
            if (firstNodeId <= 0) {
                continue;
            }
            for (const SkeletonPoint& neighbor : activeNeighbors(skeleton, x, y)) {
                const int secondNodeId = nodeByPixel[static_cast<std::size_t>(
                    indexOf(width, neighbor.x, neighbor.y))];
                if (secondNodeId <= 0 || secondNodeId == firstNodeId) {
                    continue;
                }
                const std::pair<int, int> nodePair = std::minmax(firstNodeId, secondNodeId);
                if (!directNodePairs.insert(nodePair).second) {
                    continue;
                }
                const SkeletonPoint firstAnchor = nodePair.first == firstNodeId
                    ? SkeletonPoint{x, y} : neighbor;
                const SkeletonPoint secondAnchor = nodePair.first == firstNodeId
                    ? neighbor : SkeletonPoint{x, y};

                SkeletonBranchV2 branch;
                branch.id = nextBranchId++;
                branch.startNodeId = nodePair.first;
                branch.endNodeId = nodePair.second;
                branch.points = {firstAnchor, secondAnchor};
                branch.physicalLength = metric.distance(
                    firstAnchor.x, firstAnchor.y, secondAnchor.x, secondAnchor.y);

                SkeletonPortV2 startPort;
                startPort.id = nextPortId++;
                startPort.nodeId = branch.startNodeId;
                startPort.branchId = branch.id;
                startPort.anchor = firstAnchor;
                startPort.exitPoint = secondAnchor;
                normalizedDirection(startPort.anchor, startPort.exitPoint, metric,
                                    startPort.directionX, startPort.directionY);
                SkeletonPortV2 endPort;
                endPort.id = nextPortId++;
                endPort.nodeId = branch.endNodeId;
                endPort.branchId = branch.id;
                endPort.anchor = secondAnchor;
                endPort.exitPoint = firstAnchor;
                normalizedDirection(endPort.anchor, endPort.exitPoint, metric,
                                    endPort.directionX, endPort.directionY);

                SkeletonHalfEdgeV2 forward;
                forward.id = nextHalfEdgeId++;
                forward.branchId = branch.id;
                forward.originNodeId = branch.startNodeId;
                forward.targetNodeId = branch.endNodeId;
                forward.portId = startPort.id;
                forward.physicalLength = branch.physicalLength;
                SkeletonHalfEdgeV2 reverse;
                reverse.id = nextHalfEdgeId++;
                reverse.branchId = branch.id;
                reverse.originNodeId = branch.endNodeId;
                reverse.targetNodeId = branch.startNodeId;
                reverse.portId = endPort.id;
                reverse.physicalLength = branch.physicalLength;
                forward.twinId = reverse.id;
                reverse.twinId = forward.id;
                startPort.halfEdgeId = forward.id;
                endPort.halfEdgeId = reverse.id;
                branch.startPortId = startPort.id;
                branch.endPortId = endPort.id;
                branch.forwardHalfEdgeId = forward.id;
                branch.reverseHalfEdgeId = reverse.id;
                nodeById(branch.startNodeId).portIds.push_back(startPort.id);
                nodeById(branch.endNodeId).portIds.push_back(endPort.id);
                graph.ports.push_back(startPort);
                graph.ports.push_back(endPort);
                graph.halfEdges.push_back(forward);
                graph.halfEdges.push_back(reverse);
                graph.branches.push_back(std::move(branch));
            }
        }
    }

    graph.stats.branches = static_cast<int>(graph.branches.size());
    graph.stats.ports = static_cast<int>(graph.ports.size());
    graph.stats.halfEdges = static_cast<int>(graph.halfEdges.size());
    return graph;
}

} // namespace fpt
