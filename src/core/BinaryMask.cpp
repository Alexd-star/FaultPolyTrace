#include "BinaryMask.h"

#include <algorithm>
#include <stdexcept>

namespace fpt {
namespace {

int idx(int width, int x, int y) {
    return y * width + x;
}

bool hasNeighborInRadius(const BinaryMask& mask, int x, int y, int radius) {
    for (int yy = y - radius; yy <= y + radius; ++yy) {
        for (int xx = x - radius; xx <= x + radius; ++xx) {
            if (mask.get(xx, yy)) {
                return true;
            }
        }
    }
    return false;
}

bool allNeighborsInRadius(const BinaryMask& mask, int x, int y, int radius) {
    for (int yy = y - radius; yy <= y + radius; ++yy) {
        for (int xx = x - radius; xx <= x + radius; ++xx) {
            if (!mask.get(xx, yy)) {
                return false;
            }
        }
    }
    return true;
}

int neighborCount8(const BinaryMask& mask, int x, int y) {
    int count = 0;
    for (int yy = y - 1; yy <= y + 1; ++yy) {
        for (int xx = x - 1; xx <= x + 1; ++xx) {
            if (xx == x && yy == y) {
                continue;
            }
            if (mask.get(xx, yy)) {
                ++count;
            }
        }
    }
    return count;
}

int transitionCount(const BinaryMask& mask, int x, int y) {
    const bool p2 = mask.get(x, y - 1);
    const bool p3 = mask.get(x + 1, y - 1);
    const bool p4 = mask.get(x + 1, y);
    const bool p5 = mask.get(x + 1, y + 1);
    const bool p6 = mask.get(x, y + 1);
    const bool p7 = mask.get(x - 1, y + 1);
    const bool p8 = mask.get(x - 1, y);
    const bool p9 = mask.get(x - 1, y - 1);
    const bool neighbors[9] = {p2, p3, p4, p5, p6, p7, p8, p9, p2};

    int transitions = 0;
    for (int i = 0; i < 8; ++i) {
        if (!neighbors[i] && neighbors[i + 1]) {
            ++transitions;
        }
    }
    return transitions;
}

} // namespace

BinaryMask::BinaryMask(int width, int height) : width_(width), height_(height) {
    if (width_ <= 0 || height_ <= 0) {
        throw std::runtime_error("BinaryMask dimensions must be positive.");
    }
    values_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0);
}

bool BinaryMask::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

bool BinaryMask::get(int x, int y) const {
    if (!inBounds(x, y)) {
        return false;
    }
    return values_[static_cast<std::size_t>(idx(width_, x, y))] != 0;
}

void BinaryMask::set(int x, int y, bool value) {
    if (!inBounds(x, y)) {
        return;
    }
    values_[static_cast<std::size_t>(idx(width_, x, y))] = value ? 1 : 0;
}

int BinaryMask::countActive() const {
    return static_cast<int>(std::count(values_.begin(), values_.end(), static_cast<unsigned char>(1)));
}

BinaryMask dilate(const BinaryMask& mask, int radius) {
    if (radius <= 0) {
        return mask;
    }
    BinaryMask out(mask.width(), mask.height());
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            out.set(x, y, hasNeighborInRadius(mask, x, y, radius));
        }
    }
    return out;
}

BinaryMask erode(const BinaryMask& mask, int radius) {
    if (radius <= 0) {
        return mask;
    }
    BinaryMask out(mask.width(), mask.height());
    for (int y = 0; y < mask.height(); ++y) {
        for (int x = 0; x < mask.width(); ++x) {
            out.set(x, y, allNeighborsInRadius(mask, x, y, radius));
        }
    }
    return out;
}

BinaryMask openMask(const BinaryMask& mask, int radius) {
    return dilate(erode(mask, radius), radius);
}

BinaryMask closeMask(const BinaryMask& mask, int radius) {
    return erode(dilate(mask, radius), radius);
}

BinaryMask unionMask(const BinaryMask& a, const BinaryMask& b) {
    if (a.width() != b.width() || a.height() != b.height()) {
        throw std::runtime_error("Cannot union masks with different dimensions.");
    }
    BinaryMask out(a.width(), a.height());
    for (std::size_t i = 0; i < out.values().size(); ++i) {
        out.values()[i] = (a.values()[i] || b.values()[i]) ? 1 : 0;
    }
    return out;
}

BinaryMask intersectMask(const BinaryMask& a, const BinaryMask& b) {
    if (a.width() != b.width() || a.height() != b.height()) {
        throw std::runtime_error("Cannot intersect masks with different dimensions.");
    }
    BinaryMask out(a.width(), a.height());
    for (std::size_t i = 0; i < out.values().size(); ++i) {
        out.values()[i] = (a.values()[i] && b.values()[i]) ? 1 : 0;
    }
    return out;
}

BinaryMask skeletonizeZhangSuen(const BinaryMask& mask, int maxIterations) {
    BinaryMask out = mask;
    if (out.width() < 3 || out.height() < 3) {
        return out;
    }

    bool changed = true;
    int iteration = 0;
    while (changed && iteration++ < maxIterations) {
        changed = false;
        for (int step = 0; step < 2; ++step) {
            std::vector<std::pair<int, int>> toDelete;
            for (int y = 1; y < out.height() - 1; ++y) {
                for (int x = 1; x < out.width() - 1; ++x) {
                    if (!out.get(x, y)) {
                        continue;
                    }
                    const int n = neighborCount8(out, x, y);
                    const int s = transitionCount(out, x, y);
                    const bool p2 = out.get(x, y - 1);
                    const bool p4 = out.get(x + 1, y);
                    const bool p6 = out.get(x, y + 1);
                    const bool p8 = out.get(x - 1, y);

                    const bool stepCondition = step == 0
                        ? (!p2 || !p4 || !p6) && (!p4 || !p6 || !p8)
                        : (!p2 || !p4 || !p8) && (!p2 || !p6 || !p8);

                    if (n >= 2 && n <= 6 && s == 1 && stepCondition) {
                        toDelete.push_back({x, y});
                    }
                }
            }
            if (!toDelete.empty()) {
                changed = true;
                for (const auto& p : toDelete) {
                    out.set(p.first, p.second, false);
                }
            }
        }
    }
    return out;
}

BinaryMask bufferMask(const BinaryMask& mask, int radius) {
    return dilate(mask, radius);
}

} // namespace fpt
