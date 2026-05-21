#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace fpt {

struct GridMetadata {
    int nx = 0;
    int ny = 0;
    double sx = 0.0;
    double sy = 0.0;
    double dx = 1.0;
    double dy = 1.0;
};

class Grid2D {
public:
    Grid2D() = default;
    explicit Grid2D(GridMetadata metadata);

    int nx() const { return metadata_.nx; }
    int ny() const { return metadata_.ny; }
    double sx() const { return metadata_.sx; }
    double sy() const { return metadata_.sy; }
    double dx() const { return metadata_.dx; }
    double dy() const { return metadata_.dy; }
    const GridMetadata& metadata() const { return metadata_; }

    bool empty() const { return values_.empty(); }
    std::size_t size() const { return values_.size(); }

    double value(int x, int y) const;
    double& value(int x, int y);
    bool inBounds(int x, int y) const;

    double worldX(double gridX) const;
    double worldY(double gridY) const;

    const std::vector<double>& values() const { return values_; }
    std::vector<double>& values() { return values_; }

    void validate() const;

private:
    GridMetadata metadata_;
    std::vector<double> values_;
};

} // namespace fpt

