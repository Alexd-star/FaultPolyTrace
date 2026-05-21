#include "Polygon.h"

#include <algorithm>
#include <cmath>

namespace fpt {
namespace {

double squaredDistance(const Point& a, const Point& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

double pointSegmentDistanceSquared(const Point& p, const Point& a, const Point& b) {
    const double length2 = squaredDistance(a, b);
    if (length2 <= 1e-24) {
        return squaredDistance(p, a);
    }
    double t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / length2;
    t = std::max(0.0, std::min(1.0, t));
    const Point projection{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
    return squaredDistance(p, projection);
}

void simplifyOpenPolyline(
    const std::vector<Point>& points,
    std::size_t first,
    std::size_t last,
    double toleranceSquared,
    std::vector<unsigned char>& keep) {
    if (last <= first + 1) {
        return;
    }

    double maxDistance = -1.0;
    std::size_t index = first;
    for (std::size_t i = first + 1; i < last; ++i) {
        const double distance = pointSegmentDistanceSquared(points[i], points[first], points[last]);
        if (distance > maxDistance) {
            maxDistance = distance;
            index = i;
        }
    }

    if (maxDistance > toleranceSquared) {
        keep[index] = 1;
        simplifyOpenPolyline(points, first, index, toleranceSquared, keep);
        simplifyOpenPolyline(points, index, last, toleranceSquared, keep);
    }
}

std::vector<Point> rotateRingAtStableAnchor(const std::vector<Point>& points) {
    if (points.empty()) {
        return points;
    }
    std::size_t anchor = 0;
    for (std::size_t i = 1; i < points.size(); ++i) {
        if (points[i].x < points[anchor].x ||
            (points[i].x == points[anchor].x && points[i].y < points[anchor].y)) {
            anchor = i;
        }
    }
    std::vector<Point> rotated;
    rotated.reserve(points.size());
    for (std::size_t i = 0; i < points.size(); ++i) {
        rotated.push_back(points[(anchor + i) % points.size()]);
    }
    return rotated;
}

} // namespace

double polygonArea(const std::vector<Point>& points) {
    if (points.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point& a = points[i];
        const Point& b = points[(i + 1) % points.size()];
        sum += a.x * b.y - b.x * a.y;
    }
    return std::abs(sum) * 0.5;
}

double polygonPerimeter(const std::vector<Point>& points) {
    if (points.size() < 2) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point& a = points[i];
        const Point& b = points[(i + 1) % points.size()];
        const double dx = a.x - b.x;
        const double dy = a.y - b.y;
        sum += std::sqrt(dx * dx + dy * dy);
    }
    return sum;
}

std::vector<Point> simplifyRingDouglasPeucker(const std::vector<Point>& points, double tolerance) {
    if (points.size() < 4 || tolerance <= 0.0) {
        return points;
    }

    std::vector<Point> open = rotateRingAtStableAnchor(points);
    open.push_back(open.front());

    std::vector<unsigned char> keep(open.size(), 0);
    keep.front() = 1;
    keep.back() = 1;
    simplifyOpenPolyline(open, 0, open.size() - 1, tolerance * tolerance, keep);

    std::vector<Point> simplified;
    simplified.reserve(open.size());
    for (std::size_t i = 0; i + 1 < open.size(); ++i) {
        if (keep[i]) {
            simplified.push_back(open[i]);
        }
    }
    return simplified.size() >= 3 ? simplified : points;
}

std::vector<Point> smoothRingChaikin(const std::vector<Point>& points, int iterations) {
    if (points.size() < 3 || iterations <= 0) {
        return points;
    }

    std::vector<Point> current = points;
    for (int iter = 0; iter < iterations; ++iter) {
        if (current.size() < 3 || current.size() > 200000) {
            break;
        }
        std::vector<Point> next;
        next.reserve(current.size() * 2);
        for (std::size_t i = 0; i < current.size(); ++i) {
            const Point& a = current[i];
            const Point& b = current[(i + 1) % current.size()];
            next.push_back({0.75 * a.x + 0.25 * b.x, 0.75 * a.y + 0.25 * b.y});
            next.push_back({0.25 * a.x + 0.75 * b.x, 0.25 * a.y + 0.75 * b.y});
        }
        current = std::move(next);
    }
    return current.size() >= 3 ? current : points;
}

} // namespace fpt
