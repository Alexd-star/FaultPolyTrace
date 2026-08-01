#pragma once

#include "BinaryMask.h"
#include "Grid2D.h"
#include "SkeletonGraph.h"

#include <string>
#include <vector>

namespace fpt {

struct WeakExtensionGateOptions {
    int smallComponentMaxCells = 64;
    int maxComponentCells = 20000;
    int ownerSearchRadius = 1;
    double smallMinAspect = 1.5;
    double largeMinAspect = 3.0;
    double smallMinSupportRatio = 0.30;
    double largeMinSupportRatio = 0.45;
    double smallMinMeanSupport = 0.20;
    double largeMinMeanSupport = 0.30;
    double maxEstimatedWidthSamples = 8.0;
};

struct WeakExtensionOwnerMap {
    std::vector<int> labels;
    int componentCount = 0;
};

struct WeakExtensionGateEvidence {
    int id = 0;
    bool accepted = false;
    bool smallComponent = false;
    std::string decision;
    int cellCount = 0;
    int ownerCount = 0;
    int finiteValueCount = 0;
    int invalidValueCount = 0;
    double physicalSpan = 0.0;
    double physicalAspect = 1.0;
    double estimatedWidthWorld = 0.0;
    double supportRatio = 0.0;
    double meanSupport = 0.0;
};

void validateWeakExtensionGateOptions(const WeakExtensionGateOptions& options);
WeakExtensionOwnerMap buildWeakExtensionOwnerMap(const BinaryMask& strongMask);
WeakExtensionGateEvidence evaluateWeakExtensionComponent(
    const Grid2D& grid,
    const std::vector<SkeletonPoint>& cells,
    const WeakExtensionOwnerMap& owners,
    double weakThreshold,
    double strongThreshold,
    const WeakExtensionGateOptions& options);

} // namespace fpt
