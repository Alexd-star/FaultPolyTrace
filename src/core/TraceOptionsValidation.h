#pragma once

namespace fpt {

class Grid2D;
struct TraceOptions;

void validateTraceRequest(
    const Grid2D& grid,
    const TraceOptions& options);

} // namespace fpt
