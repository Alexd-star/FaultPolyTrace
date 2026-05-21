#pragma once

#include <vector>

namespace fpt {

class BinaryMask {
public:
    BinaryMask() = default;
    BinaryMask(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    bool empty() const { return values_.empty(); }

    bool inBounds(int x, int y) const;
    bool get(int x, int y) const;
    void set(int x, int y, bool value);
    int countActive() const;

    const std::vector<unsigned char>& values() const { return values_; }
    std::vector<unsigned char>& values() { return values_; }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<unsigned char> values_;
};

BinaryMask dilate(const BinaryMask& mask, int radius);
BinaryMask erode(const BinaryMask& mask, int radius);
BinaryMask openMask(const BinaryMask& mask, int radius);
BinaryMask closeMask(const BinaryMask& mask, int radius);
BinaryMask unionMask(const BinaryMask& a, const BinaryMask& b);
BinaryMask intersectMask(const BinaryMask& a, const BinaryMask& b);
BinaryMask skeletonizeZhangSuen(const BinaryMask& mask, int maxIterations = 256);
BinaryMask bufferMask(const BinaryMask& mask, int radius);

} // namespace fpt
