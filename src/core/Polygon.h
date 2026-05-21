#pragma once

#include <vector>

namespace fpt {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct Polygon {
    int id = 0;
    std::vector<Point> points;
    double area = 0.0;
    double perimeter = 0.0;
    int cellCount = 0;
    double meanValue = 0.0;
    double maxValue = 0.0;
    bool kept = true;
};

double polygonArea(const std::vector<Point>& points);
double polygonPerimeter(const std::vector<Point>& points);
std::vector<Point> simplifyRingDouglasPeucker(const std::vector<Point>& points, double tolerance);
std::vector<Point> smoothRingChaikin(const std::vector<Point>& points, int iterations);

} // namespace fpt
