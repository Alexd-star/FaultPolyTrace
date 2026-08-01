#pragma once

#include "BinaryMask.h"

#include <vector>

namespace fpt {

class Grid2D;
struct Polygon;
struct TraceOptions;
struct TraceResult;

BinaryMask rasterizeKeptPolygonCoverage(
    const Grid2D& grid,
    const std::vector<Polygon>& polygons,
    const BinaryMask* candidateMask = nullptr);

void appendPolygonsFromMask(
    const Grid2D& grid,
    const BinaryMask& activeMask,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId);

void appendPolygonsFromLocalMask(
    const Grid2D& grid,
    const std::vector<unsigned char>& active,
    int minX,
    int minY,
    int localWidth,
    int localHeight,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId,
    BinaryMask* keptCoverageMask = nullptr);

void appendResidualTracePolygons(
    const Grid2D& grid,
    const BinaryMask& residualMask,
    const std::vector<int>& groupOwners,
    const BinaryMask& alreadyCoveredMask,
    const BinaryMask& candidateCoveredMask,
    const TraceOptions& options,
    TraceResult& result,
    int& nextId);

} // namespace fpt
