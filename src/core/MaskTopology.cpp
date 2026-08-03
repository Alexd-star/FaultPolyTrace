#include "MaskTopology.h"

#include <queue>
#include <stdexcept>
#include <vector>

namespace fpt {
namespace {

constexpr int kNeighbors4[4][2] = {
    {-1, 0}, {1, 0}, {0, -1}, {0, 1}
};

void requireMatchingMasks(const BinaryMask& a, const BinaryMask& b) {
    if (a.width() != b.width() || a.height() != b.height()) {
        throw std::runtime_error("Topology masks must have matching dimensions.");
    }
}

int countComponents4(const BinaryMask& mask) {
    const int width = mask.width();
    const int height = mask.height();
    std::vector<unsigned char> visited(mask.values().size(), 0);
    std::queue<int> queue;
    int components = 0;
    for (int y0 = 0; y0 < height; ++y0) {
        for (int x0 = 0; x0 < width; ++x0) {
            const int start = y0 * width + x0;
            if (!mask.get(x0, y0) || visited[static_cast<std::size_t>(start)]) {
                continue;
            }
            ++components;
            visited[static_cast<std::size_t>(start)] = 1;
            queue.push(start);
            while (!queue.empty()) {
                const int index = queue.front();
                queue.pop();
                const int x = index % width;
                const int y = index / width;
                for (const auto& offset : kNeighbors4) {
                    const int nx = x + offset[0];
                    const int ny = y + offset[1];
                    if (!mask.inBounds(nx, ny) || !mask.get(nx, ny)) {
                        continue;
                    }
                    const int neighbor = ny * width + nx;
                    if (visited[static_cast<std::size_t>(neighbor)]) {
                        continue;
                    }
                    visited[static_cast<std::size_t>(neighbor)] = 1;
                    queue.push(neighbor);
                }
            }
        }
    }
    return components;
}

} // namespace

BinaryMask exteriorInactiveCells(const BinaryMask& mask) {
    const int width = mask.width();
    const int height = mask.height();
    BinaryMask exterior(width, height);
    std::queue<int> queue;
    auto seed = [&](int x, int y) {
        if (!mask.get(x, y) && !exterior.get(x, y)) {
            exterior.set(x, y, true);
            queue.push(y * width + x);
        }
    };
    for (int x = 0; x < width; ++x) {
        seed(x, 0);
        seed(x, height - 1);
    }
    for (int y = 0; y < height; ++y) {
        seed(0, y);
        seed(width - 1, y);
    }
    while (!queue.empty()) {
        const int index = queue.front();
        queue.pop();
        const int x = index % width;
        const int y = index / width;
        for (const auto& offset : kNeighbors4) {
            const int nx = x + offset[0];
            const int ny = y + offset[1];
            if (!mask.inBounds(nx, ny) || mask.get(nx, ny) ||
                exterior.get(nx, ny)) {
                continue;
            }
            exterior.set(nx, ny, true);
            queue.push(ny * width + nx);
        }
    }
    return exterior;
}

int countInactiveHoles(const BinaryMask& mask) {
    const BinaryMask exterior = exteriorInactiveCells(mask);
    BinaryMask holes(mask.width(), mask.height());
    for (std::size_t index = 0; index < holes.values().size(); ++index) {
        holes.values()[index] =
            (!mask.values()[index] && !exterior.values()[index]) ? 1 : 0;
    }
    return countComponents4(holes);
}

NewInactiveEnclosureAnalysis analyzeNewInactiveEnclosures(
    const BinaryMask& baseMask,
    const BinaryMask& candidateMask) {
    requireMatchingMasks(baseMask, candidateMask);
    const BinaryMask baseExterior = exteriorInactiveCells(baseMask);
    const BinaryMask candidateExterior = exteriorInactiveCells(candidateMask);
    NewInactiveEnclosureAnalysis analysis{
        BinaryMask(baseMask.width(), baseMask.height()), 0, 0};
    for (std::size_t index = 0; index < analysis.cells.values().size(); ++index) {
        const bool newlyEnclosed =
            baseExterior.values()[index] &&
            !candidateExterior.values()[index] &&
            !candidateMask.values()[index];
        if (newlyEnclosed) {
            analysis.cells.values()[index] = 1;
            ++analysis.cellCount;
        }
    }
    analysis.componentCount = countComponents4(analysis.cells);
    return analysis;
}

} // namespace fpt
