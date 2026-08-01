#include "../src/core/FaultTracer.h"
#include "../src/core/ProductDefaults.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    fpt::TraceOptions options;
    fpt::product_defaults::applyVisibleProductionDefaults(options);
    require(std::abs(options.percentile - 0.98) < 1e-12,
            "The product percentile must come from the shared profile.");
    require(std::abs(options.minArea - 120.0) < 1e-12,
            "The product area threshold must come from the shared profile.");
    require(options.rootAntiMerge,
            "The released production profile must enable root anti-merge.");
    require(!options.residualTrace,
            "Residual tracing must remain opt-in in the released profile.");
    require(options.residualFillRadius == 4 &&
                options.residualTraceMaxCells == 80,
            "Residual recovery limits must match the released profile.");
    require(options.simplifyTolerance == 2.0 && options.smoothIterations == 1,
            "Released geometry post-processing defaults must stay synchronized.");
    std::cout << "Product default profile tests passed.\n";
    return 0;
}
