#pragma once

#include "Grid2D.h"
#include "TraceOptions.h"
#include "TraceResult.h"

namespace fpt {

TraceResult traceFaultPolygons(const Grid2D& grid, const TraceOptions& options);

} // namespace fpt
