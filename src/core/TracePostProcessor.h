#pragma once

namespace fpt {

class BinaryMask;
class Grid2D;
struct TraceOptions;
struct TraceResult;

void applyTracePostProcessing(
    const Grid2D& grid,
    const BinaryMask& attributeMask,
    const TraceOptions& options,
    TraceResult& result);

} // namespace fpt
