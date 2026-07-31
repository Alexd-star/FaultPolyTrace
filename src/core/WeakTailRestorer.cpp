#include "WeakTailRestorer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <stdexcept>

namespace fpt {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct AxisEstimate {
    double x = 1.0;
    double y = 0.0;
    double major = 0.0;
    double minor = 0.0;
};

AxisEstimate estimateAxis(const std::vector<SkeletonPoint>& points) {
    AxisEstimate estimate;
    if (points.size() < 2) {
        return estimate;
    }
    double meanX = 0.0;
    double meanY = 0.0;
    for (const SkeletonPoint& point : points) {
        meanX += point.x;
        meanY += point.y;
    }
    meanX /= static_cast<double>(points.size());
    meanY /= static_cast<double>(points.size());

    double xx = 0.0;
    double xy = 0.0;
    double yy = 0.0;
    for (const SkeletonPoint& point : points) {
        const double dx = point.x - meanX;
        const double dy = point.y - meanY;
        xx += dx * dx;
        xy += dx * dy;
        yy += dy * dy;
    }
    const double norm = std::max(1.0, static_cast<double>(points.size() - 1));
    xx /= norm;
    xy /= norm;
    yy /= norm;
    const double trace = xx + yy;
    const double determinant = xx * yy - xy * xy;
    const double discriminant = std::sqrt(std::max(0.0, trace * trace - 4.0 * determinant));
    estimate.major = std::max(0.0, 0.5 * (trace + discriminant));
    estimate.minor = std::max(0.0, 0.5 * (trace - discriminant));
    if (std::abs(xy) > 1e-12) {
        estimate.x = estimate.major - yy;
        estimate.y = xy;
    } else if (yy > xx) {
        estimate.x = 0.0;
        estimate.y = 1.0;
    }
    const double length = std::hypot(estimate.x, estimate.y);
    if (length > 1e-12) {
        estimate.x /= length;
        estimate.y /= length;
    }
    return estimate;
}

double undirectedAngleDegrees(const AxisEstimate& a, const AxisEstimate& b) {
    const double cosine = std::max(0.0, std::min(1.0, std::abs(a.x * b.x + a.y * b.y)));
    return std::acos(cosine) * 180.0 / kPi;
}

std::vector<int> labelComponents(const BinaryMask& mask) {
    const int width = mask.width();
    const int height = mask.height();
    std::vector<int> labels(static_cast<std::size_t>(width * height), 0);
    std::queue<SkeletonPoint> queue;
    int nextLabel = 0;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const int index = y * width + x;
            if (!mask.get(x, y) || labels[static_cast<std::size_t>(index)] != 0) {
                continue;
            }
            labels[static_cast<std::size_t>(index)] = ++nextLabel;
            queue.push({x, y});
            while (!queue.empty()) {
                const SkeletonPoint point = queue.front();
                queue.pop();
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const int nx = point.x + ox;
                        const int ny = point.y + oy;
                        if (!mask.inBounds(nx, ny) || !mask.get(nx, ny)) {
                            continue;
                        }
                        const int neighbor = ny * width + nx;
                        if (labels[static_cast<std::size_t>(neighbor)] != 0) {
                            continue;
                        }
                        labels[static_cast<std::size_t>(neighbor)] = nextLabel;
                        queue.push({nx, ny});
                    }
                }
            }
        }
    }
    return labels;
}

} // namespace

void validateWeakTailOptions(const WeakTailOptions& options) {
    if (options.minCells < 1 || options.maxCells < options.minCells ||
        options.minAspect < 1.0 || options.minMeanSupportRatio < 0.0 ||
        options.minPeakSupportRatio < 0.0 ||
        options.maxAlignmentAngleDegrees < 0.0 ||
        options.maxAlignmentAngleDegrees > 90.0 ||
        options.ownershipClearanceRadius < 1 || options.tangentRadius < 1 ||
        options.maxEvidenceRecords < 1) {
        throw std::runtime_error("Weak tail restoration options are out of range.");
    }
}

WeakTailResult restoreSupportedWeakTails(
    const Grid2D& grid,
    const BinaryMask& skeleton,
    const BinaryMask& keptSkeleton,
    double supportThreshold,
    const WeakTailOptions& options) {
    validateWeakTailOptions(options);
    if (skeleton.width() != keptSkeleton.width() ||
        skeleton.height() != keptSkeleton.height() ||
        skeleton.width() != grid.nx() || skeleton.height() != grid.ny()) {
        throw std::runtime_error("Weak tail restoration masks must match the grid.");
    }

    WeakTailResult result{
        BinaryMask(keptSkeleton),
        BinaryMask(grid.nx(), grid.ny()),
        {},
        {}};
    const int width = grid.nx();
    const int height = grid.ny();
    const std::vector<int> ownerLabels = labelComponents(keptSkeleton);
    std::vector<unsigned char> visited(static_cast<std::size_t>(width * height), 0);
    auto indexOf = [width](int x, int y) { return y * width + x; };

    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const int startIndex = indexOf(x0, y0);
            if (visited[static_cast<std::size_t>(startIndex)] ||
                !skeleton.get(x0, y0) || keptSkeleton.get(x0, y0)) {
                continue;
            }
            ++result.stats.componentCount;
            WeakTailCandidate candidate;
            candidate.id = result.stats.componentCount;
            candidate.minX = candidate.maxX = x0;
            candidate.minY = candidate.maxY = y0;
            candidate.maxValue = -std::numeric_limits<double>::infinity();
            std::queue<SkeletonPoint> queue;
            queue.push({x0, y0});
            visited[static_cast<std::size_t>(startIndex)] = 1;
            double sum = 0.0;
            while (!queue.empty()) {
                const SkeletonPoint point = queue.front();
                queue.pop();
                candidate.cells.push_back(point);
                candidate.minX = std::min(candidate.minX, point.x);
                candidate.maxX = std::max(candidate.maxX, point.x);
                candidate.minY = std::min(candidate.minY, point.y);
                candidate.maxY = std::max(candidate.maxY, point.y);
                const double value = grid.value(point.x, point.y);
                if (std::isfinite(value)) {
                    sum += value;
                    candidate.maxValue = std::max(candidate.maxValue, value);
                }
                for (int oy = -1; oy <= 1; ++oy) {
                    for (int ox = -1; ox <= 1; ++ox) {
                        if (ox == 0 && oy == 0) {
                            continue;
                        }
                        const int nx = point.x + ox;
                        const int ny = point.y + oy;
                        if (!skeleton.inBounds(nx, ny) || !skeleton.get(nx, ny) ||
                            keptSkeleton.get(nx, ny)) {
                            continue;
                        }
                        const int neighbor = indexOf(nx, ny);
                        if (visited[static_cast<std::size_t>(neighbor)]) {
                            continue;
                        }
                        visited[static_cast<std::size_t>(neighbor)] = 1;
                        queue.push({nx, ny});
                    }
                }
            }

            candidate.cellCount = static_cast<int>(candidate.cells.size());
            candidate.meanValue = candidate.cells.empty()
                ? 0.0 : sum / static_cast<double>(candidate.cells.size());
            candidate.span = std::hypot(
                static_cast<double>(candidate.maxX - candidate.minX),
                static_cast<double>(candidate.maxY - candidate.minY));
            const AxisEstimate tailAxis = estimateAxis(candidate.cells);
            candidate.aspect = std::sqrt(
                (tailAxis.major + 1e-9) / (tailAxis.minor + 1e-9));

            std::set<int> touchedOwners;
            for (const SkeletonPoint& point : candidate.cells) {
                for (int oy = -options.ownershipClearanceRadius;
                     oy <= options.ownershipClearanceRadius; ++oy) {
                    for (int ox = -options.ownershipClearanceRadius;
                         ox <= options.ownershipClearanceRadius; ++ox) {
                        if (ox * ox + oy * oy >
                            options.ownershipClearanceRadius * options.ownershipClearanceRadius) {
                            continue;
                        }
                        const int nx = point.x + ox;
                        const int ny = point.y + oy;
                        if (!keptSkeleton.inBounds(nx, ny)) {
                            continue;
                        }
                        const int label = ownerLabels[static_cast<std::size_t>(indexOf(nx, ny))];
                        if (label > 0) {
                            touchedOwners.insert(label);
                        }
                    }
                }
            }
            candidate.touchedOwnerCount = static_cast<int>(touchedOwners.size());
            if (touchedOwners.size() == 1) {
                candidate.ownerLabel = *touchedOwners.begin();
            }

            std::vector<SkeletonPoint> localOwnerPoints;
            if (candidate.ownerLabel > 0) {
                const int minX = std::max(0, candidate.minX - options.tangentRadius);
                const int maxX = std::min(width - 1, candidate.maxX + options.tangentRadius);
                const int minY = std::max(0, candidate.minY - options.tangentRadius);
                const int maxY = std::min(height - 1, candidate.maxY + options.tangentRadius);
                for (int y = minY; y <= maxY; ++y) {
                    for (int x = minX; x <= maxX; ++x) {
                        if (ownerLabels[static_cast<std::size_t>(indexOf(x, y))] ==
                            candidate.ownerLabel) {
                            localOwnerPoints.push_back({x, y});
                        }
                    }
                }
            }
            if (localOwnerPoints.size() >= 4) {
                candidate.alignmentAngleDegrees = undirectedAngleDegrees(
                    tailAxis, estimateAxis(localOwnerPoints));
            }

            if (candidate.cellCount < options.minCells ||
                candidate.cellCount > options.maxCells) {
                candidate.decision = "size_out_of_range";
                ++result.stats.sizeRejected;
            } else if (candidate.aspect < options.minAspect ||
                       candidate.span < static_cast<double>(options.minCells - 1)) {
                candidate.decision = "not_linear_enough";
                ++result.stats.shapeRejected;
            } else if (candidate.meanValue < supportThreshold * options.minMeanSupportRatio ||
                       candidate.maxValue < supportThreshold * options.minPeakSupportRatio) {
                candidate.decision = "insufficient_attribute_support";
                ++result.stats.attributeRejected;
            } else if (candidate.touchedOwnerCount != 1) {
                candidate.decision = "ambiguous_or_detached_owner";
                ++result.stats.ownershipRejected;
            } else if (localOwnerPoints.size() < 4 ||
                       candidate.alignmentAngleDegrees > options.maxAlignmentAngleDegrees) {
                candidate.decision = "direction_discontinuity";
                ++result.stats.directionRejected;
            } else {
                candidate.accepted = true;
                candidate.decision = "accepted";
                ++result.stats.acceptedComponents;
                result.stats.acceptedCells += candidate.cellCount;
                for (const SkeletonPoint& point : candidate.cells) {
                    result.mask.set(point.x, point.y, true);
                    result.acceptedMask.set(point.x, point.y, true);
                }
            }
            if (static_cast<int>(result.candidates.size()) < options.maxEvidenceRecords) {
                result.candidates.push_back(std::move(candidate));
            }
        }
    }
    return result;
}

} // namespace fpt
